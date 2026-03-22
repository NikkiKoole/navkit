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

// ============================================================================
// VOICFORM (4-formant voice with phoneme interpolation)
// ============================================================================

// Phoneme table — Klatt 1980 / Peterson & Barney 1952 formant data
// F1-F4 frequencies, bandwidths, amplitudes for a male adult voice
static const VFPhonemeEntry vfPhonemeTable[VF_PHONEME_COUNT] = {
    // Vowels — voiced, no noise
    //              F1     F2     F3     F4        BW1   BW2   BW3   BW4     A1    A2    A3    A4    noise  voiced
    [VF_A]  = {{ 730, 1090, 2440, 3400}, { 90, 110, 140, 180}, {1.0f,0.5f,0.3f,0.1f}, 0.0f, true},
    [VF_E]  = {{ 530, 1840, 2480, 3400}, { 70, 100, 130, 170}, {1.0f,0.7f,0.3f,0.1f}, 0.0f, true},
    [VF_I]  = {{ 270, 2290, 3010, 3400}, { 60,  90, 120, 170}, {1.0f,0.5f,0.2f,0.1f}, 0.0f, true},
    [VF_O]  = {{ 570,  840, 2410, 3400}, { 80, 100, 140, 180}, {1.0f,0.4f,0.2f,0.1f}, 0.0f, true},
    [VF_U]  = {{ 300,  870, 2240, 3400}, { 65,  90, 130, 170}, {1.0f,0.3f,0.15f,0.08f},0.0f, true},
    [VF_AE] = {{ 660, 1720, 2410, 3400}, { 80, 110, 140, 180}, {1.0f,0.6f,0.3f,0.1f}, 0.0f, true},
    [VF_AH] = {{ 520, 1190, 2390, 3400}, { 75, 100, 130, 170}, {1.0f,0.45f,0.25f,0.1f},0.0f, true},
    [VF_AW] = {{ 570,  840, 2410, 3400}, { 80, 100, 140, 180}, {1.0f,0.4f,0.2f,0.1f}, 0.0f, true},
    [VF_UH] = {{ 440, 1020, 2240, 3400}, { 70,  95, 130, 170}, {1.0f,0.4f,0.2f,0.08f},0.0f, true},
    [VF_ER] = {{ 490, 1350, 1690, 3400}, { 75, 100, 120, 170}, {1.0f,0.5f,0.4f,0.1f}, 0.0f, true},
    // Nasals/liquids/glides — voiced, nasal has low F1, anti-formant
    [VF_M]  = {{ 200, 1200, 2500, 3400}, {100, 120, 150, 200}, {0.3f,0.15f,0.1f,0.05f},0.0f, true},
    [VF_N]  = {{ 200, 1700, 2500, 3400}, {100, 120, 150, 200}, {0.3f,0.2f,0.1f,0.05f}, 0.0f, true},
    [VF_NG] = {{ 200, 1700, 2700, 3400}, {100, 120, 150, 200}, {0.25f,0.15f,0.1f,0.05f},0.0f,true},
    [VF_L]  = {{ 350, 1050, 2400, 3400}, { 80, 100, 130, 170}, {0.6f,0.3f,0.2f,0.1f}, 0.0f, true},
    [VF_R]  = {{ 420, 1300, 1600, 3400}, { 80, 100, 120, 170}, {0.5f,0.3f,0.3f,0.1f}, 0.0f, true},
    [VF_W]  = {{ 300,  610, 2200, 3400}, { 65,  90, 130, 170}, {0.5f,0.2f,0.1f,0.05f}, 0.0f, true},
    [VF_Y]  = {{ 260, 2070, 3020, 3400}, { 60,  90, 130, 170}, {0.5f,0.3f,0.15f,0.05f},0.0f, true},
    [VF_DH] = {{ 300, 1700, 2500, 3400}, { 80, 110, 140, 180}, {0.4f,0.2f,0.15f,0.05f},0.15f,true},
    // Fricatives — unvoiced (noise-excited), or voiced+noise
    [VF_F]  = {{ 300, 1700, 3500, 5500}, {100, 150, 200, 300}, {0.1f,0.1f,0.2f,0.15f}, 0.5f, false},
    [VF_V]  = {{ 300, 1700, 3500, 5500}, {100, 150, 200, 300}, {0.2f,0.15f,0.2f,0.1f}, 0.35f, true},
    [VF_S]  = {{ 300, 1700, 5000, 7000}, {100, 150, 250, 400}, {0.05f,0.05f,0.3f,0.4f},0.9f, false},
    [VF_Z]  = {{ 300, 1700, 5000, 7000}, {100, 150, 250, 400}, {0.15f,0.1f,0.25f,0.3f},0.6f, true},
    [VF_SH] = {{ 300, 1700, 3800, 6000}, {100, 150, 200, 350}, {0.05f,0.05f,0.35f,0.3f},0.85f,false},
    [VF_ZH] = {{ 300, 1700, 3800, 6000}, {100, 150, 200, 350}, {0.15f,0.1f,0.3f,0.2f}, 0.5f, true},
    [VF_TH] = {{ 300, 1700, 4500, 6500}, {100, 150, 250, 350}, {0.08f,0.08f,0.15f,0.1f},0.4f, false},
    // Plosives — short burst, noise-only
    [VF_B]  = {{ 300, 1100, 2500, 3400}, {100, 120, 150, 200}, {0.3f,0.2f,0.1f,0.05f}, 0.3f, true},
    [VF_D]  = {{ 300, 1700, 2500, 3400}, {100, 120, 150, 200}, {0.2f,0.25f,0.15f,0.05f},0.4f, true},
    [VF_G]  = {{ 300, 1700, 2700, 3400}, {100, 120, 150, 200}, {0.2f,0.2f,0.2f,0.05f}, 0.35f, true},
    [VF_P]  = {{ 300, 1100, 2500, 3400}, {100, 120, 150, 200}, {0.1f,0.1f,0.05f,0.02f},0.6f, false},
    [VF_T]  = {{ 300, 1700, 3500, 5500}, {100, 120, 200, 300}, {0.1f,0.1f,0.15f,0.1f}, 0.7f, false},
    [VF_K]  = {{ 300, 1700, 3000, 4500}, {100, 120, 180, 250}, {0.1f,0.15f,0.15f,0.08f},0.6f, false},
    [VF_CH] = {{ 300, 1700, 3800, 6000}, {100, 120, 200, 350}, {0.1f,0.1f,0.25f,0.2f}, 0.8f, false},
};

// Phoneme name strings for DAW UI
static const char* vfPhonemeNames[] = {
    "A","E","I","O","U","AE","AH","AW","UH","ER",
    "M","N","NG","L","R","W","Y","DH",
    "F","V","S","Z","SH","ZH","TH",
    "B","D","G","P","T","K","CH"
};

// Process VoicForm oscillator: LF glottal source → 4 formant SVFs → phoneme morph
static float processVoicFormOscillator(Voice *v, float sampleRate) {
    VoicFormSettings *vf = &v->voicformSettings;

    // Step 1: Advance phoneme morph
    if (vf->phonemeCurrent != vf->phonemeTarget) {
        vf->morphBlend += vf->morphRate / sampleRate;
        if (vf->morphBlend >= 1.0f) {
            vf->phonemeCurrent = vf->phonemeTarget;
            vf->morphBlend = 0.0f;
        }
    }

    // Step 2: Interpolate phoneme formant parameters
    int pc = vf->phonemeCurrent;
    int pt = vf->phonemeTarget;
    if (pc < 0 || pc >= VF_PHONEME_COUNT) pc = VF_A;
    if (pt < 0 || pt >= VF_PHONEME_COUNT) pt = pc;
    const VFPhonemeEntry *cur = &vfPhonemeTable[pc];
    const VFPhonemeEntry *tgt = &vfPhonemeTable[pt];
    float b = vf->morphBlend;
    float ib = 1.0f - b;

    float freq4[4], bw4[4], amp4[4];
    for (int i = 0; i < 4; i++) {
        freq4[i] = (cur->freq[i] * ib + tgt->freq[i] * b) * vf->formantShift;
        bw4[i] = cur->bw[i] * ib + tgt->bw[i] * b;
        amp4[i] = cur->amp[i] * ib + tgt->amp[i] * b;
    }
    float noiseGain = cur->noiseGain * ib + tgt->noiseGain * b;
    bool voiced = cur->voiced || tgt->voiced;

    // Step 3: Vibrato
    vf->vibratoPhase += vf->vibratoRate / sampleRate;
    if (vf->vibratoPhase >= 1.0f) vf->vibratoPhase -= 1.0f;
    float vibrato = sinf(vf->vibratoPhase * 2.0f * PI) * vf->vibratoDepth;
    float freqMod = v->frequency * powf(2.0f, vibrato / 12.0f);

    // Step 4: Glottal source — Rosenberg polynomial pulse
    float source = 0.0f;
    if (voiced) {
        v->phase += freqMod / sampleRate;
        if (v->phase >= 1.0f) v->phase -= 1.0f;
        float t = v->phase;
        float oq = vf->openQuotient; // Tp/T0 ratio (0.3-0.7)
        float te = oq + 0.1f;        // Closure ends slightly after peak
        if (te > 0.95f) te = 0.95f;

        if (t < oq) {
            // Open phase: Rosenberg polynomial (smooth rise to peak)
            float tn = t / oq;
            source = 3.0f * tn * tn - 2.0f * tn * tn * tn;
        } else if (t < te) {
            // Closing phase: half-cosine fall
            float tn = (t - oq) / (te - oq);
            source = 0.5f * (1.0f + cosf(tn * PI));
        } else {
            // Closed phase
            source = 0.0f;
        }

        // Spectral tilt: 1-pole LP darkens the source
        // tilt 0 = bright (bypass), tilt 1 = very dark
        float tiltCoeff = 0.2f + vf->spectralTilt * 0.75f;
        vf->tiltState += tiltCoeff * (source - vf->tiltState);
        source = source * (1.0f - vf->spectralTilt * 0.7f) + vf->tiltState * vf->spectralTilt * 0.7f;
    }

    // Step 5: Aspiration noise — mix with glottal
    float aspirationNoise = noise() * vf->aspiration;
    source = source * (1.0f - vf->aspiration * 0.5f) + aspirationNoise;

    // Also add phoneme-specific noise (fricatives)
    if (noiseGain > 0.001f) {
        source += noise() * noiseGain * 0.7f;
    }

    // Step 6: Consonant burst at note-on (decays over ~20ms)
    if (vf->burstLevel > 0.001f) {
        vf->burstTime += 1.0f / sampleRate;
        float burstDuration = 0.025f; // 25ms burst
        if (vf->burstTime < burstDuration) {
            float burstEnv = vf->burstLevel * (1.0f - vf->burstTime / burstDuration);
            source += noise() * burstEnv * 0.8f;
        } else {
            vf->burstLevel = 0.0f;
        }
    }

    // Step 7: Process through 4 formant filters and sum
    float out = 0.0f;
    for (int i = 0; i < 4; i++) {
        // Update formant filter frequency and bandwidth
        vf->formants[i].freq = freq4[i];
        vf->formants[i].bw = bw4[i];
        out += processFormantFilter(&vf->formants[i], source, sampleRate) * amp4[i];
    }

    return out * 0.7f;
}

// Karplus-Strong plucked string oscillator
// With allpass fractional delay for accurate pitch tuning (Jaffe & Smith 1983)
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

    // First-order allpass for fractional delay tuning (Jaffe & Smith 1983)
    // Transfer function: H(z) = (C + z^-1) / (1 + C*z^-1)
    // Direct form I: y[n] = C*x[n] + x[n-1] - C*y[n-1]
    // Using transposed direct form II (one state variable):
    //   y[n] = C*x[n] + s;  s = x[n] - C*y[n]
    float apOut = v->ksAllpassCoeff * filtered + v->ksAllpassState;
    v->ksAllpassState = filtered - v->ksAllpassCoeff * apOut;

    // Write back to delay line
    v->ksBuffer[v->ksIndex] = apOut;
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

// Reed instrument oscillator — single/double reed waveguide model
// Based on McIntyre/Schumacher/Woodhouse (1983) pressure-driven reed valve
// coupled to a cylindrical or conical bore. The reed acts as a nonlinear
// reflection function at the mouthpiece end of the bore waveguide.
//
// Physics:
//   mouth pressure (Pm) → reed gap → flow into bore → bore waveguide → bell
//   The reed displacement x = (Pm - Pbore) / stiffness, clamped to [0,1].
//   Flow through the gap: U = x * sqrt(|Pm - Pbore|) * sign(Pm - Pbore)
//   This is the Bernoulli flow model. We use a polynomial approximation:
//   U = x * (1 - x) which captures the essential nonlinear valve behavior —
//   flow increases with opening but drops as the reed closes against the lay.
//
// Bore: single delay line. Cylindrical bore (clarinet) overblows at the 12th
// (3rd harmonic) due to closed-end reflection. Conical bore (sax) overblows
// at the octave. The bore parameter crossfades between these reflection types.
//
// References:
//   - McIntyre, Schumacher & Woodhouse, "On the Oscillations of Musical
//     Instruments" (JASA 1983) — the canonical reed oscillation model
//   - Bilbao, "Numerical Sound Synthesis" (2009) — reed valve discretization
//   - Cook, "Real Sound Synthesis for Interactive Applications" (2002), Ch. 10
//   - Scavone (STK Clarinet/Saxofony classes)
//   - Guillemain et al., "Digital synthesis of self-sustained instruments" (2005)
static float processReedOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    ReedSettings *rs = &v->reedSettings;
    if (rs->boreLen <= 0) return 0.0f;

    // Lip vibrato — subtle pressure modulation (5-7 Hz typical)
    float vibrato = 0.0f;
    if (rs->vibratoDepth > 0.0f) {
        rs->vibratoPhase += 5.8f / sampleRate;
        if (rs->vibratoPhase >= 1.0f) rs->vibratoPhase -= 1.0f;
        vibrato = sinf(rs->vibratoPhase * 2.0f * PI) * rs->vibratoDepth * 0.1f;
    }

    // Mouth pressure with vibrato
    float Pm = rs->blowPressure + vibrato;

    // === Bore delay line: read returning wave ===
    float boreReturn = rs->boreBuf[rs->boreIdx];

    // === Bell-end filtering ===
    // Bore shape dramatically affects the tone:
    //
    // Cylindrical bore (clarinet, bore=0): The closed reed end + open bell
    // create a half-wave resonator with ODD harmonics only. The bore walls
    // absorb highs, giving a dark, hollow, woody tone. Heavy LP filtering.
    //
    // Conical bore (sax, bore=0.7-0.9): Acts like open-open pipe, ALL harmonics
    // present. The flaring bell radiates highs efficiently. Less LP filtering,
    // brighter tone, more "edge."
    //
    // Narrow conical (oboe, bore=0.5-0.6): Still all harmonics but the narrow
    // bore emphasizes upper partials → nasal, penetrating quality.

    // 1-pole LP: bore wall + radiation losses
    // Cylindrical: heavy filtering (dark, hollow) → lpCoeff = 0.55
    // Conical: light filtering (bright, edgy) → lpCoeff = 0.92
    float lpCoeff = 0.55f + rs->bore * 0.37f;
    rs->lpState = rs->lpState * (1.0f - lpCoeff) + boreReturn * lpCoeff;

    // Open-end reflection coefficient
    // All bore types invert at the open bell end, but loss varies:
    // Cylindrical: more loss at bell (less radiation) → -0.95
    // Conical: flared bell radiates more → -0.85
    float endRefl = -0.95f + rs->bore * 0.10f;
    float boreFiltered = rs->lpState;

    // === Reed reflection function ===
    // pressureDiff = reflected bore pressure - mouth pressure
    float pressureDiff = endRefl * boreFiltered - Pm;

    // Reed table: offset + slope * input, clamped to [-1, 1]
    //
    // Aperture controls the rest opening — how much air flows with no bore
    // pressure. Wide aperture (harmonica) = easy to blow, warm fundamental.
    // Narrow aperture (oboe) = resistant, bright, complex harmonics.
    //
    // Stiffness controls how the reed responds to pressure changes.
    // Soft reed: gentle response, smooth tone, fewer upper harmonics
    // Stiff reed: aggressive response, bright attack, rich harmonics, nasal edge
    float reedOffset = 0.3f + rs->aperture * 0.4f;  // 0.3-0.7: wide range
    float reedSlope = -0.4f - rs->stiffness * 0.5f;  // -0.4 to -0.9: dramatic range
    float reedRefl = reedOffset + reedSlope * pressureDiff;
    reedRefl = clampf(reedRefl, -1.0f, 1.0f);

    // Bore input: mouth pressure + reflected pressure modulated by reed
    float boreInput = Pm + pressureDiff * reedRefl;

    // === Even harmonic injection for conical bores ===
    // Cylindrical bores naturally suppress even harmonics (odd-only resonance).
    // Conical bores support all harmonics. To emphasize this difference,
    // add a small nonlinear term that generates even harmonics for conical bores.
    // tanh(x) is odd-symmetric but tanh(x + bias) breaks symmetry → even harmonics.
    if (rs->bore > 0.3f) {
        float conicalDrive = (rs->bore - 0.3f) * 0.7f; // 0 at bore=0.3, ~0.5 at bore=1
        boreInput = tanhf(boreInput * (1.0f + conicalDrive * 2.0f)) / (1.0f + conicalDrive * 2.0f);
        // Asymmetric clip for even harmonics (sax "buzz")
        boreInput += conicalDrive * boreInput * boreInput * 0.3f;
    }

    // Write into bore delay line
    rs->boreBuf[rs->boreIdx] = boreInput;

    // Advance bore waveguide
    rs->boreIdx = (rs->boreIdx + 1) % rs->boreLen;

    // Output: pressure at bell (bore output before reflection)
    float out = boreReturn;

    // DC blocking (essential — reed model has large DC from steady blow pressure)
    float dcOut = out - rs->dcPrev + 0.995f * rs->dcState;
    rs->dcPrev = out;
    rs->dcState = dcOut;

    return dcOut * 1.5f;
}

// Brass instrument oscillator — lip-valve waveguide model
// Based on Adachi & Sato (1995), Cook "Real Sound Synthesis" (2002), STK Brass.
//
// Physics:
//   The player's lips act as a pressure-controlled oscillator (mass-spring system)
//   coupled to a bore waveguide. Unlike a reed (which is a passive valve driven by
//   bore pressure), the lips have their own resonant frequency that must be near a
//   bore resonance for oscillation to occur — this is "mode locking."
//
//   blow pressure (Pm) → lip oscillator → flow into bore → bore waveguide → bell
//
//   The lip model: a damped harmonic oscillator whose equilibrium position is
//   modulated by the pressure difference across the lips. The flow through the
//   lip gap is proportional to lip opening × pressure difference.
//
// Bore types:
//   Cylindrical (trumpet, bore=0): bright, projecting, strong upper harmonics
//   Conical (French horn, bore=1): warm, mellow, complex, weaker upper harmonics
//   Bell flare: all brass instruments have a flare, modeled as a 2-pole LP
//
// Mute: reduces high frequencies and adds a nasal quality (harmon mute effect)
//
// References:
//   - Adachi & Sato, "Time-domain simulation of sound production in the brass
//     instrument" (JASA 1995) — lip oscillator model
//   - Cook, "Real Sound Synthesis for Interactive Applications" (2002), Ch. 10
//   - Scavone, STK Brass class
//   - Vergez & Rodet, "Trumpet and trumpet player: model and simulation in a
//     musical context" (ICMC 1997) — lip dynamics
//   - Campbell, Gilbert & Myers, "The Science of Brass Instruments" (Springer 2021)
static float processBrassOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    BrassSettings *bs = &v->brassSettings;
    if (bs->boreLen <= 0) return 0.0f;

    float Pm = bs->blowPressure;

    // === Bore delay line: read returning wave ===
    float boreReturn = bs->boreBuf[bs->boreIdx];

    // === Bell-end processing (STK-style: single LP + reflection) ===
    // One LP filter for bore wall + bell radiation losses.
    // Bore parameter controls the cutoff:
    //   Cylindrical (trumpet, bore=0): bright → high LP coeff (passes more HF)
    //   Conical (horn, bore=1): dark, mellow → lower LP coeff (rolls off HF)
    float lpCoeff = 0.7f + (1.0f - bs->bore) * 0.2f; // 0.90 trumpet, 0.70 horn
    bs->lpState = bs->lpState * (1.0f - lpCoeff) + boreReturn * lpCoeff;

    // Bell reflection: invert + small loss (STK uses -0.95)
    float pMinus = bs->lpState * -0.95f;

    // Mute: additional LP stage that darkens and nasalizes
    if (bs->mute > 0.01f) {
        float muteCoeff = 1.0f - bs->mute * 0.5f;
        bs->lpState2 = bs->lpState2 * (1.0f - muteCoeff) + pMinus * muteCoeff;
        pMinus = bs->lpState2;
    }

    // === Lip reflection (STK memoryless nonlinearity) ===
    // The lip has no resonant frequency — the bore delay alone sets the pitch.
    // The nonlinear function sustains oscillation and shapes harmonics.
    //
    // Reed-proven structure:
    //   pressureDiff = reflected_bore - blow_pressure
    //   lipRefl = table(pressureDiff)
    //   boreInput = Pm + pressureDiff * lipRefl
    //
    // Brass character: tanh soft-saturation (smoother than reed's hard clamp)
    // + lipTension scales the nonlinearity for brighter/darker timbres

    float pressureDiff = pMinus - Pm;

    // Lip table with tanh saturation
    float lipOffset = 0.35f + bs->lipAperture * 0.3f;   // 0.35-0.65
    float lipSlope = -0.5f - bs->lipTension * 0.3f;      // -0.5 to -0.8
    float lipRefl = lipOffset + lipSlope * pressureDiff;
    lipRefl = tanhf(lipRefl * (1.2f + bs->lipTension * 0.8f));

    // Bore input: blow pressure + pressure difference × lip reflection
    float boreInput = Pm + pressureDiff * lipRefl;

    // Write into bore delay line
    bs->boreBuf[bs->boreIdx] = boreInput;

    // Advance bore waveguide
    bs->boreIdx = (bs->boreIdx + 1) % bs->boreLen;

    // Output: radiated sound at bell (bore return after LP = what radiates)
    float out = bs->lpState;

    // DC blocking
    float dcOut = out - bs->dcPrev + 0.995f * bs->dcState;
    bs->dcPrev = out;
    bs->dcState = dcOut;

    return dcOut * 4.0f;
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
            // Tuned bar ratios (Fletcher & Rossing): undercut brings mode 2 to 4:1 (two octaves)
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 4.0f;    // 2 octaves (undercut-tuned, industry standard)
            ms->modeFreqs[2] = 10.0f;   // ~3.2 octaves
            ms->modeFreqs[3] = 20.0f;   // ~4.3 octaves (barely audible)
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.25f;
            ms->modeAmpsInit[2] = 0.08f;
            ms->modeAmpsInit[3] = 0.02f;
            ms->modeDecays[0] = 2.5f;   // Long fundamental decay
            ms->modeDecays[1] = 1.2f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.2f;
            ms->stiffness = 0.0f;       // Ratios already tuned, no stiffness stretch needed
            ms->hardness = 0.4f;        // Medium-soft mallets
            ms->strikePos = 0.3f;       // Slightly off-center
            ms->resonance = 0.8f;       // Strong resonator tubes
            break;

        case MALLET_PRESET_VIBES:
            // Vibraphone: metallic, sustaining, motor tremolo
            // Tuned bar ratios: mode 2 at 4:1 (two octaves), like marimba but brighter
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 4.0f;    // 2 octaves (tuned aluminum bar)
            ms->modeFreqs[2] = 10.0f;
            ms->modeFreqs[3] = 20.0f;
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.4f;
            ms->modeAmpsInit[2] = 0.2f;
            ms->modeAmpsInit[3] = 0.1f;
            ms->modeDecays[0] = 4.0f;   // Very long sustain
            ms->modeDecays[1] = 3.0f;
            ms->modeDecays[2] = 2.0f;
            ms->modeDecays[3] = 1.0f;
            ms->stiffness = 0.0f;       // Ratios already tuned
            ms->hardness = 0.5f;        // Medium mallets
            ms->strikePos = 0.25f;
            ms->resonance = 0.9f;
            ms->tremolo = 0.5f;         // Motor tremolo on
            ms->tremoloRate = 5.5f;
            break;

        case MALLET_PRESET_XYLOPHONE:
            // Xylophone: bright, sharp attack, short decay
            // Tuned bar ratios: mode 2 at 3:1 (octave + fifth), brighter than marimba
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 3.0f;    // Octave + fifth (rosewood bar tuning)
            ms->modeFreqs[2] = 9.0f;    // 3 octaves + major second
            ms->modeFreqs[3] = 18.0f;
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.5f;
            ms->modeAmpsInit[2] = 0.3f;
            ms->modeAmpsInit[3] = 0.15f;
            ms->modeDecays[0] = 0.8f;   // Short decay
            ms->modeDecays[1] = 0.5f;
            ms->modeDecays[2] = 0.3f;
            ms->modeDecays[3] = 0.15f;
            ms->stiffness = 0.0f;       // Ratios already tuned
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
            // Tubular bell partials (nearly harmonic tube modes)
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 2.0f;
            ms->modeFreqs[2] = 3.0f;
            ms->modeFreqs[3] = 4.0f;
            ms->stiffness = 0.0f;       // Ratios already set, no stretch
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
        
        // Read from buffer with cubic Hermite interpolation
        float readPos = g->bufferPos + g->position * table->size;

        // Wrap around buffer
        while (readPos >= table->size) readPos -= table->size;
        while (readPos < 0) readPos += table->size;

        float sample = cubicHermite(table->data, table->size, readPos);
        
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

// ============================================================================
// METALLIC PERCUSSION (ring-mod of square wave pairs — 808/909/cymbal/bell)
// ============================================================================

// Classic 808 hihat ratios: 6 square oscillators ring-modulated in pairs
// Reference: Roland TR-808 service notes — 6 metal-square oscillators
static const float metallic808Ratios[6] = {
    1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f
};

// 909 uses slightly different ratios with more harmonic content
static const float metallic909Ratios[6] = {
    1.0f, 1.4953f, 1.6388f, 1.9533f, 2.5316f, 2.7074f
};

// Initialize metallic percussion with preset
static void initMetallicPreset(MetallicSettings *ms, MetallicPreset preset) {
    ms->preset = preset;

    // Reset phases and noise state
    for (int i = 0; i < 6; i++) {
        ms->modePhases[i] = 0.0f;
    }
    ms->noiseLPState = 0.0f;
    ms->pitchEnvTime = 0.0f;

    switch (preset) {
        case METALLIC_808_CH:
            // 808 closed hihat: tight, sizzly, short decay
            for (int i = 0; i < 6; i++) ms->ratios[i] = metallic808Ratios[i];
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.9f;
            ms->modeAmpsInit[2] = 0.8f; ms->modeAmpsInit[3] = 0.7f;
            ms->modeAmpsInit[4] = 0.6f; ms->modeAmpsInit[5] = 0.5f;
            ms->modeDecays[0] = 0.08f; ms->modeDecays[1] = 0.06f;
            ms->modeDecays[2] = 0.05f; ms->modeDecays[3] = 0.04f;
            ms->modeDecays[4] = 0.03f; ms->modeDecays[5] = 0.025f;
            ms->ringMix = 0.85f;
            ms->noiseLevel = 0.15f;
            ms->noiseHPCutoff = 0.6f;
            ms->brightness = 1.0f;      // Full square waves
            ms->pitchEnvAmount = 1.5f;   // Subtle attack transient
            ms->pitchEnvDecay = 0.005f;
            break;

        case METALLIC_808_OH:
            // 808 open hihat: same character, longer ring
            for (int i = 0; i < 6; i++) ms->ratios[i] = metallic808Ratios[i];
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.9f;
            ms->modeAmpsInit[2] = 0.8f; ms->modeAmpsInit[3] = 0.7f;
            ms->modeAmpsInit[4] = 0.6f; ms->modeAmpsInit[5] = 0.5f;
            ms->modeDecays[0] = 0.5f;  ms->modeDecays[1] = 0.4f;
            ms->modeDecays[2] = 0.35f; ms->modeDecays[3] = 0.3f;
            ms->modeDecays[4] = 0.25f; ms->modeDecays[5] = 0.2f;
            ms->ringMix = 0.85f;
            ms->noiseLevel = 0.12f;
            ms->noiseHPCutoff = 0.55f;
            ms->brightness = 1.0f;
            ms->pitchEnvAmount = 1.5f;
            ms->pitchEnvDecay = 0.005f;
            break;

        case METALLIC_909_CH:
            // 909 closed hihat: brighter, more noise
            for (int i = 0; i < 6; i++) ms->ratios[i] = metallic909Ratios[i];
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.85f;
            ms->modeAmpsInit[2] = 0.75f; ms->modeAmpsInit[3] = 0.65f;
            ms->modeAmpsInit[4] = 0.55f; ms->modeAmpsInit[5] = 0.45f;
            ms->modeDecays[0] = 0.06f; ms->modeDecays[1] = 0.05f;
            ms->modeDecays[2] = 0.04f; ms->modeDecays[3] = 0.035f;
            ms->modeDecays[4] = 0.03f; ms->modeDecays[5] = 0.025f;
            ms->ringMix = 0.9f;
            ms->noiseLevel = 0.25f;
            ms->noiseHPCutoff = 0.65f;
            ms->brightness = 1.0f;
            ms->pitchEnvAmount = 2.0f;
            ms->pitchEnvDecay = 0.003f;
            break;

        case METALLIC_909_OH:
            // 909 open hihat: bright, washy
            for (int i = 0; i < 6; i++) ms->ratios[i] = metallic909Ratios[i];
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.85f;
            ms->modeAmpsInit[2] = 0.75f; ms->modeAmpsInit[3] = 0.65f;
            ms->modeAmpsInit[4] = 0.55f; ms->modeAmpsInit[5] = 0.45f;
            ms->modeDecays[0] = 0.6f;  ms->modeDecays[1] = 0.5f;
            ms->modeDecays[2] = 0.4f;  ms->modeDecays[3] = 0.35f;
            ms->modeDecays[4] = 0.3f;  ms->modeDecays[5] = 0.25f;
            ms->ringMix = 0.9f;
            ms->noiseLevel = 0.3f;
            ms->noiseHPCutoff = 0.6f;
            ms->brightness = 1.0f;
            ms->pitchEnvAmount = 2.0f;
            ms->pitchEnvDecay = 0.003f;
            break;

        case METALLIC_RIDE:
            // Ride cymbal: higher ratios, long shimmer, less noise
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 2.12f;
            ms->ratios[2] = 3.36f;  ms->ratios[3] = 4.15f;
            ms->ratios[4] = 5.43f;  ms->ratios[5] = 6.28f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.7f;
            ms->modeAmpsInit[2] = 0.5f; ms->modeAmpsInit[3] = 0.35f;
            ms->modeAmpsInit[4] = 0.25f; ms->modeAmpsInit[5] = 0.15f;
            ms->modeDecays[0] = 3.0f;  ms->modeDecays[1] = 2.5f;
            ms->modeDecays[2] = 2.0f;  ms->modeDecays[3] = 1.5f;
            ms->modeDecays[4] = 1.0f;  ms->modeDecays[5] = 0.7f;
            ms->ringMix = 0.7f;
            ms->noiseLevel = 0.08f;
            ms->noiseHPCutoff = 0.7f;
            ms->brightness = 0.7f;      // Slightly softer than square
            ms->pitchEnvAmount = 0.5f;
            ms->pitchEnvDecay = 0.008f;
            break;

        case METALLIC_CRASH:
            // Crash cymbal: bright burst, very long tail
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 2.24f;
            ms->ratios[2] = 3.55f;  ms->ratios[3] = 4.62f;
            ms->ratios[4] = 5.81f;  ms->ratios[5] = 7.07f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.8f;
            ms->modeAmpsInit[2] = 0.7f; ms->modeAmpsInit[3] = 0.5f;
            ms->modeAmpsInit[4] = 0.4f; ms->modeAmpsInit[5] = 0.3f;
            ms->modeDecays[0] = 5.0f;  ms->modeDecays[1] = 4.0f;
            ms->modeDecays[2] = 3.0f;  ms->modeDecays[3] = 2.0f;
            ms->modeDecays[4] = 1.5f;  ms->modeDecays[5] = 1.0f;
            ms->ringMix = 0.75f;
            ms->noiseLevel = 0.2f;
            ms->noiseHPCutoff = 0.5f;
            ms->brightness = 0.8f;
            ms->pitchEnvAmount = 3.0f;
            ms->pitchEnvDecay = 0.01f;
            break;

        case METALLIC_COWBELL:
            // 808 cowbell: 2 detuned squares, no noise, sine-ish
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 1.504f;  // Classic 800/540 Hz ratio
            ms->ratios[2] = 0.0f;   ms->ratios[3] = 0.0f;    // Only 1 pair active
            ms->ratios[4] = 0.0f;   ms->ratios[5] = 0.0f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.8f;
            ms->modeAmpsInit[2] = 0.0f; ms->modeAmpsInit[3] = 0.0f;
            ms->modeAmpsInit[4] = 0.0f; ms->modeAmpsInit[5] = 0.0f;
            ms->modeDecays[0] = 0.6f;  ms->modeDecays[1] = 0.5f;
            ms->modeDecays[2] = 0.0f;  ms->modeDecays[3] = 0.0f;
            ms->modeDecays[4] = 0.0f;  ms->modeDecays[5] = 0.0f;
            ms->ringMix = 0.0f;        // Additive for cowbell (ring-mod sounds wrong)
            ms->noiseLevel = 0.0f;
            ms->noiseHPCutoff = 0.5f;
            ms->brightness = 1.0f;
            ms->pitchEnvAmount = 0.0f;
            ms->pitchEnvDecay = 0.01f;
            break;

        case METALLIC_BELL:
            // Tubular bell / chime: pure sine modes, long sustain
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 2.0f;
            ms->ratios[2] = 3.0f;   ms->ratios[3] = 4.2f;
            ms->ratios[4] = 5.4f;   ms->ratios[5] = 6.8f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.6f;
            ms->modeAmpsInit[2] = 0.35f; ms->modeAmpsInit[3] = 0.2f;
            ms->modeAmpsInit[4] = 0.12f; ms->modeAmpsInit[5] = 0.06f;
            ms->modeDecays[0] = 4.0f;  ms->modeDecays[1] = 3.0f;
            ms->modeDecays[2] = 2.0f;  ms->modeDecays[3] = 1.5f;
            ms->modeDecays[4] = 1.0f;  ms->modeDecays[5] = 0.7f;
            ms->ringMix = 0.3f;        // Slight ring-mod for inharmonicity
            ms->noiseLevel = 0.0f;
            ms->noiseHPCutoff = 0.5f;
            ms->brightness = 0.0f;      // Pure sine modes
            ms->pitchEnvAmount = 0.0f;
            ms->pitchEnvDecay = 0.01f;
            break;

        case METALLIC_GONG:
            // Tam-tam / gong: very low, long decay, pitch drop
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 1.32f;
            ms->ratios[2] = 1.65f;  ms->ratios[3] = 2.14f;
            ms->ratios[4] = 2.76f;  ms->ratios[5] = 3.45f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.7f;
            ms->modeAmpsInit[2] = 0.5f; ms->modeAmpsInit[3] = 0.35f;
            ms->modeAmpsInit[4] = 0.25f; ms->modeAmpsInit[5] = 0.15f;
            ms->modeDecays[0] = 8.0f;  ms->modeDecays[1] = 7.0f;
            ms->modeDecays[2] = 5.0f;  ms->modeDecays[3] = 4.0f;
            ms->modeDecays[4] = 3.0f;  ms->modeDecays[5] = 2.0f;
            ms->ringMix = 0.5f;
            ms->noiseLevel = 0.05f;
            ms->noiseHPCutoff = 0.3f;
            ms->brightness = 0.2f;      // Mostly sine, slight edge
            ms->pitchEnvAmount = 4.0f;  // Characteristic pitch drop
            ms->pitchEnvDecay = 0.05f;
            break;

        case METALLIC_AGOGO:
            // Agogo bell: bright, short, two prominent modes
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 2.8f;
            ms->ratios[2] = 4.1f;   ms->ratios[3] = 0.0f;
            ms->ratios[4] = 0.0f;   ms->ratios[5] = 0.0f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.5f;
            ms->modeAmpsInit[2] = 0.2f; ms->modeAmpsInit[3] = 0.0f;
            ms->modeAmpsInit[4] = 0.0f; ms->modeAmpsInit[5] = 0.0f;
            ms->modeDecays[0] = 1.0f;  ms->modeDecays[1] = 0.6f;
            ms->modeDecays[2] = 0.3f;  ms->modeDecays[3] = 0.0f;
            ms->modeDecays[4] = 0.0f;  ms->modeDecays[5] = 0.0f;
            ms->ringMix = 0.4f;
            ms->noiseLevel = 0.0f;
            ms->noiseHPCutoff = 0.5f;
            ms->brightness = 0.5f;
            ms->pitchEnvAmount = 1.0f;
            ms->pitchEnvDecay = 0.003f;
            break;

        case METALLIC_TRIANGLE:
        default:
            // Triangle (instrument): very pure, long ring, almost sine
            ms->ratios[0] = 1.0f;   ms->ratios[1] = 2.76f;
            ms->ratios[2] = 5.40f;  ms->ratios[3] = 0.0f;
            ms->ratios[4] = 0.0f;   ms->ratios[5] = 0.0f;
            ms->modeAmpsInit[0] = 1.0f; ms->modeAmpsInit[1] = 0.15f;
            ms->modeAmpsInit[2] = 0.05f; ms->modeAmpsInit[3] = 0.0f;
            ms->modeAmpsInit[4] = 0.0f; ms->modeAmpsInit[5] = 0.0f;
            ms->modeDecays[0] = 6.0f;  ms->modeDecays[1] = 3.0f;
            ms->modeDecays[2] = 1.5f;  ms->modeDecays[3] = 0.0f;
            ms->modeDecays[4] = 0.0f;  ms->modeDecays[5] = 0.0f;
            ms->ringMix = 0.2f;
            ms->noiseLevel = 0.02f;
            ms->noiseHPCutoff = 0.8f;
            ms->brightness = 0.0f;      // Pure sine
            ms->pitchEnvAmount = 0.0f;
            ms->pitchEnvDecay = 0.01f;
            break;
    }

    // Copy initial amplitudes
    for (int i = 0; i < 6; i++) {
        ms->modeAmps[i] = ms->modeAmpsInit[i];
    }
}

// Process metallic percussion oscillator
// Core DSP: 6 oscillators in 3 ring-mod pairs with per-partial decay
static float processMetallicOscillator(Voice *v, float sampleRate) {
    MetallicSettings *ms = &v->metallicSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;

    // Pitch envelope (attack transient — pitch drops quickly after strike)
    float pitchMult = 1.0f;
    if (ms->pitchEnvAmount > 0.0f && ms->pitchEnvDecay > 0.0f) {
        float envVal = expf(-ms->pitchEnvTime / ms->pitchEnvDecay);
        pitchMult = 1.0f + (ms->pitchEnvAmount / 12.0f) * envVal;  // semitones to ratio
        ms->pitchEnvTime += dt;
    }

    // Generate 6 oscillator samples
    float osc[6];
    for (int i = 0; i < 6; i++) {
        if (ms->modeAmps[i] < 0.0001f || ms->ratios[i] <= 0.0f) {
            osc[i] = 0.0f;
            continue;
        }

        float freq = v->frequency * ms->ratios[i] * pitchMult;

        // Skip if above Nyquist
        if (freq >= sampleRate * 0.5f) {
            osc[i] = 0.0f;
            continue;
        }

        // Advance phase
        ms->modePhases[i] += freq * dt;
        if (ms->modePhases[i] >= 1.0f) ms->modePhases[i] -= 1.0f;

        // Generate waveform: crossfade sine → square based on brightness
        float sine = sinf(ms->modePhases[i] * 2.0f * PI);
        if (ms->brightness <= 0.001f) {
            osc[i] = sine;
        } else {
            float square = (ms->modePhases[i] < 0.5f) ? 1.0f : -1.0f;
            osc[i] = sine + ms->brightness * (square - sine);
        }

        // Per-oscillator exponential decay
        float decayRate = 1.0f / fmaxf(ms->modeDecays[i], 0.001f);
        ms->modeAmps[i] *= (1.0f - decayRate * dt);
        if (ms->modeAmps[i] < 0.00001f) ms->modeAmps[i] = 0.0f;
    }

    // Mix: ring-mod pairs + additive blend
    // Ring-mod: osc[0]*osc[1], osc[2]*osc[3], osc[4]*osc[5]
    // Additive: sum of all oscillators weighted by amplitude
    float ringOut = 0.0f;
    float addOut = 0.0f;

    for (int p = 0; p < 3; p++) {
        int i0 = p * 2;
        int i1 = p * 2 + 1;
        float a0 = ms->modeAmps[i0];
        float a1 = ms->modeAmps[i1];
        if (a0 > 0.0001f && a1 > 0.0001f) {
            ringOut += osc[i0] * osc[i1] * a0 * a1;
        }
        addOut += osc[i0] * a0 + osc[i1] * a1;
    }

    // Normalize additive (6 oscillators) vs ring (3 pairs)
    addOut *= 0.25f;
    ringOut *= 0.5f;

    out = addOut + ms->ringMix * (ringOut - addOut);

    // HP-filtered noise layer (sizzle)
    if (ms->noiseLevel > 0.001f) {
        float n = noise();
        // One-pole LP then subtract for HP
        ms->noiseLPState += ms->noiseHPCutoff * (n - ms->noiseLPState);
        float hpNoise = n - ms->noiseLPState;
        out += hpNoise * ms->noiseLevel;
    }

    return out * 0.5f;
}

// ============================================================================
// GUITAR BODY (KS string + body resonator biquads)
// ============================================================================

// Set up a biquad bandpass filter for body resonance
static void initBodyBiquad(BodyBiquad *bq, float centerFreq, float bandwidth, float gain, float sampleRate) {
    float w0 = 2.0f * PI * centerFreq / sampleRate;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 * sinhf(logf(2.0f) / 2.0f * bandwidth * w0 / sinW0);

    float a0 = 1.0f + alpha;
    bq->b0 = (alpha * gain) / a0;
    bq->b1 = 0.0f;
    bq->b2 = (-alpha * gain) / a0;
    bq->a1 = (-2.0f * cosW0) / a0;
    bq->a2 = (1.0f - alpha) / a0;
    bq->z1 = 0.0f;
    bq->z2 = 0.0f;
}

// Process one sample through biquad (transposed direct form II)
static inline float processBodyBiquad(BodyBiquad *bq, float in) {
    float out = bq->b0 * in + bq->z1;
    bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
    bq->z2 = bq->b2 * in - bq->a2 * out;
    return out;
}

// Initialize guitar body resonator for a given preset
static void initGuitarPreset(GuitarSettings *gs, GuitarPreset preset, float freq, float sampleRate) {
    gs->preset = preset;
    gs->buzzState = 0.0f;

    // Body formant frequencies, bandwidths, and gains per preset
    // These model the resonant peaks of the instrument body
    // Reference: Elejabarrieta et al. (2002), guitar body mode measurements
    float formants[GUITAR_BODY_MODES];    // Center frequencies (Hz)
    float bandwidths[GUITAR_BODY_MODES];  // Bandwidth in octaves
    float gains[GUITAR_BODY_MODES];       // Relative gain

    (void)freq;  // Formants are body-fixed, not pitch-dependent

    switch (preset) {
        case GUITAR_ACOUSTIC:
            // Steel-string acoustic: strong low-mid body, ~100/200/400/800 Hz
            formants[0] = 98.0f;   bandwidths[0] = 0.8f; gains[0] = 1.0f;
            formants[1] = 204.0f;  bandwidths[1] = 0.6f; gains[1] = 0.8f;
            formants[2] = 390.0f;  bandwidths[2] = 0.5f; gains[2] = 0.5f;
            formants[3] = 810.0f;  bandwidths[3] = 0.4f; gains[3] = 0.3f;
            gs->bodyMix = 0.6f;
            gs->bodyBrightness = 0.5f;
            gs->pickPosition = 0.3f;
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9985f;   // Medium sustain (steel strings)
            gs->stringBrightness = 0.5f;   // Balanced
            break;

        case GUITAR_CLASSICAL:
            // Nylon: warmer, broader low resonance, softer highs
            formants[0] = 90.0f;   bandwidths[0] = 1.0f; gains[0] = 1.0f;
            formants[1] = 185.0f;  bandwidths[1] = 0.7f; gains[1] = 0.9f;
            formants[2] = 350.0f;  bandwidths[2] = 0.6f; gains[2] = 0.4f;
            formants[3] = 700.0f;  bandwidths[3] = 0.5f; gains[3] = 0.2f;
            gs->bodyMix = 0.65f;
            gs->bodyBrightness = 0.35f;
            gs->pickPosition = 0.45f;      // Pluck nearer to center
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9975f;   // Shorter sustain (nylon damps faster)
            gs->stringBrightness = 0.25f;  // Warm, mellow — nylon absorbs highs
            break;

        case GUITAR_BANJO:
            // Banjo: membrane body, sharp midrange peak, bright twang
            formants[0] = 260.0f;  bandwidths[0] = 0.4f; gains[0] = 1.0f;
            formants[1] = 480.0f;  bandwidths[1] = 0.3f; gains[1] = 0.7f;
            formants[2] = 920.0f;  bandwidths[2] = 0.3f; gains[2] = 0.5f;
            formants[3] = 1800.0f; bandwidths[3] = 0.4f; gains[3] = 0.25f;
            gs->bodyMix = 0.75f;
            gs->bodyBrightness = 0.7f;
            gs->pickPosition = 0.15f;      // Very near bridge — maximum twang
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9990f;   // Medium-long (membrane resonates)
            gs->stringBrightness = 0.8f;   // Bright, metallic attack
            break;

        case GUITAR_SITAR:
            // Sitar: gourd resonator, strong low formant, jawari buzz
            formants[0] = 80.0f;   bandwidths[0] = 0.6f; gains[0] = 1.0f;
            formants[1] = 170.0f;  bandwidths[1] = 0.5f; gains[1] = 0.7f;
            formants[2] = 420.0f;  bandwidths[2] = 0.4f; gains[2] = 0.4f;
            formants[3] = 1100.0f; bandwidths[3] = 0.5f; gains[3] = 0.35f;
            gs->bodyMix = 0.55f;
            gs->bodyBrightness = 0.6f;
            gs->pickPosition = 0.25f;
            gs->buzzAmount = 0.6f;         // Jawari bridge buzz
            gs->stringDamping = 0.9990f;   // Long sustain (steel + jawari sustains)
            gs->stringBrightness = 0.55f;  // Medium-bright
            break;

        case GUITAR_OUD:
            // Oud: deep round body, strong low resonance, short dark notes
            formants[0] = 75.0f;   bandwidths[0] = 0.9f; gains[0] = 1.0f;
            formants[1] = 155.0f;  bandwidths[1] = 0.7f; gains[1] = 0.85f;
            formants[2] = 310.0f;  bandwidths[2] = 0.5f; gains[2] = 0.45f;
            formants[3] = 620.0f;  bandwidths[3] = 0.4f; gains[3] = 0.2f;
            gs->bodyMix = 0.75f;           // Strong body (round bowl resonator)
            gs->bodyBrightness = 0.3f;
            gs->pickPosition = 0.35f;
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9965f;   // Short sustain (gut strings, heavy damping)
            gs->stringBrightness = 0.2f;   // Very dark — gut strings, deep body
            break;

        case GUITAR_KOTO:
            // Koto: bright, percussive attack, long ring, hard bridges
            formants[0] = 130.0f;  bandwidths[0] = 0.5f; gains[0] = 0.7f;
            formants[1] = 350.0f;  bandwidths[1] = 0.3f; gains[1] = 1.0f;
            formants[2] = 850.0f;  bandwidths[2] = 0.3f; gains[2] = 0.6f;
            formants[3] = 2200.0f; bandwidths[3] = 0.4f; gains[3] = 0.3f;
            gs->bodyMix = 0.5f;
            gs->bodyBrightness = 0.8f;
            gs->pickPosition = 0.12f;      // Very close to bridge — max brightness
            gs->buzzAmount = 0.1f;         // Slight sawari buzz
            gs->stringDamping = 0.9992f;   // Long ring (silk strings, hard bridges)
            gs->stringBrightness = 0.85f;  // Very bright — hard plectrum, bridge contact
            break;

        case GUITAR_HARP:
            // Harp: minimal body, very long sustain, clear and open
            formants[0] = 100.0f;  bandwidths[0] = 1.2f; gains[0] = 0.3f;
            formants[1] = 250.0f;  bandwidths[1] = 1.0f; gains[1] = 0.2f;
            formants[2] = 500.0f;  bandwidths[2] = 0.8f; gains[2] = 0.15f;
            formants[3] = 1000.0f; bandwidths[3] = 0.6f; gains[3] = 0.1f;
            gs->bodyMix = 0.15f;           // Very little body color
            gs->bodyBrightness = 0.55f;
            gs->pickPosition = 0.5f;       // Center pluck (finger near middle)
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9995f;   // Very long sustain (concert harp rings)
            gs->stringBrightness = 0.6f;   // Clear, open — not dark, not harsh
            break;

        case GUITAR_UKULELE:
        default:
            // Ukulele: small body, warm but percussive, short sustain
            formants[0] = 180.0f;  bandwidths[0] = 0.7f; gains[0] = 1.0f;
            formants[1] = 380.0f;  bandwidths[1] = 0.5f; gains[1] = 0.75f;
            formants[2] = 720.0f;  bandwidths[2] = 0.4f; gains[2] = 0.4f;
            formants[3] = 1400.0f; bandwidths[3] = 0.4f; gains[3] = 0.2f;
            gs->bodyMix = 0.65f;
            gs->bodyBrightness = 0.45f;
            gs->pickPosition = 0.4f;
            gs->buzzAmount = 0.0f;
            gs->stringDamping = 0.9970f;   // Short sustain (nylon + small body)
            gs->stringBrightness = 0.35f;  // Warm — nylon strings, small resonator
            break;
    }

    // Initialize body biquads
    for (int i = 0; i < GUITAR_BODY_MODES; i++) {
        // Scale high formant gains by brightness
        float brightScale = (i < 2) ? 1.0f : gs->bodyBrightness;
        initBodyBiquad(&gs->body[i], formants[i], bandwidths[i],
                       gains[i] * brightScale, sampleRate);
    }
}

// Apply pick position to KS excitation buffer
// Near bridge (0): all harmonics present -> bright, twangy
// Near center (1): odd harmonics suppressed -> warm, round
// Comb-notch filter on the initial noise burst
static void applyPickPosition(Voice *v, float pickPos) {
    if (v->ksLength <= 0) return;

    // Pick position creates a notch at harmonics of (sampleRate / pickPos)
    int pickSample = (int)(pickPos * (float)v->ksLength);
    if (pickSample < 1) pickSample = 1;
    if (pickSample >= v->ksLength) pickSample = v->ksLength - 1;

    // In-place comb filter on noise burst
    // Temp copy since we read from positions we'll overwrite
    float temp[2048];
    for (int i = 0; i < v->ksLength; i++) {
        temp[i] = v->ksBuffer[i];
    }
    for (int i = 0; i < v->ksLength; i++) {
        int delayed = (i + pickSample) % v->ksLength;
        v->ksBuffer[i] = (temp[i] + temp[delayed]) * 0.5f;
    }
}

// Process guitar oscillator: KS string -> body resonator -> optional buzz
static float processGuitarOscillator(Voice *v, float sampleRate) {
    if (v->ksLength <= 0) return 0.0f;
    GuitarSettings *gs = &v->guitarSettings;

    // Step 1: Run Karplus-Strong string with per-preset damping/brightness
    float stringSample = v->ksBuffer[v->ksIndex];

    int nextIndex = (v->ksIndex + 1) % v->ksLength;
    float nextSample = v->ksBuffer[nextIndex];

    // Use per-preset string character (overrides global pluck params)
    float bright = (gs->stringBrightness >= 0.0f) ? gs->stringBrightness : v->ksBrightness;
    float avg = (stringSample + nextSample) * 0.5f;
    float blended = avg + (stringSample - avg) * bright;
    float damp = pluckDamp;
    float filtered = v->ksLastSample + (blended - v->ksLastSample) * (1.0f - damp * 0.8f);
    float damping = (gs->stringDamping > 0.0f) ? gs->stringDamping : v->ksDamping;
    filtered *= damping;
    v->ksLastSample = filtered;

    // Step 2: Jawari bridge — amplitude-dependent delay modulation
    // When the string vibrates with large amplitude, it wraps around the curved
    // bridge surface, shortening the effective vibrating length (raising pitch).
    // As amplitude decays, the string lifts off and pitch drops back — this creates
    // the characteristic descending "precursor" harmonic sweep of sitar/tanpura.
    float apCoeff = v->ksAllpassCoeff;
    if (gs->buzzAmount > 0.001f) {
        float amplitude = fabsf(filtered);
        // Parabolic bridge profile: length reduction ∝ amplitude²
        // Modulate the allpass coefficient to shorten effective delay
        float mod = amplitude * amplitude * gs->buzzAmount * 1.5f;
        if (mod > 0.9f) mod = 0.9f;
        // Reducing the allpass coeff shortens the fractional delay → higher pitch
        apCoeff = apCoeff * (1.0f - mod);
        if (apCoeff < -0.99f) apCoeff = -0.99f;
        if (apCoeff > 0.99f) apCoeff = 0.99f;
    }

    // Allpass fractional delay tuning (with jawari modulation)
    float apOut = apCoeff * filtered + v->ksAllpassState;
    v->ksAllpassState = filtered - apCoeff * apOut;

    // DC blocker in the loop (prevents DC buildup on long-sustain presets like harp)
    float dcOut = apOut - gs->dcPrev + 0.995f * gs->dcState;
    gs->dcPrev = apOut;
    gs->dcState = dcOut;

    v->ksBuffer[v->ksIndex] = dcOut;
    v->ksIndex = nextIndex;

    float buzzed = stringSample;

    // Step 3: Body resonator - parallel biquad bandpass filters
    float bodyOut = 0.0f;
    for (int i = 0; i < GUITAR_BODY_MODES; i++) {
        bodyOut += processBodyBiquad(&gs->body[i], buzzed);
    }
    bodyOut *= 0.35f;  // Normalize (4 parallel bandpasses)

    // Step 4: Mix dry string + wet body
    (void)sampleRate;
    float out = buzzed + gs->bodyMix * (bodyOut - buzzed * 0.3f);

    return out;
}

// ============================================================================
// STIFKARP (stiff string — piano/harpsichord/dulcimer)
// ============================================================================

// Shape excitation noise burst based on hammer hardness
// Soft hammer = mellow (LP filtered), hard hammer = bright (raw noise)
static void initStifKarpExcitation(Voice *v, float hardness) {
    if (v->ksLength <= 0) return;

    // Low-pass filter the noise burst to simulate hammer felt
    // hardness 0 = very soft felt (heavy filtering), 1 = hard steel (minimal filtering)
    float cutoff = 0.05f + hardness * 0.85f;
    float state = 0.0f;
    for (int i = 0; i < v->ksLength; i++) {
        state += cutoff * (v->ksBuffer[i] - state);
        v->ksBuffer[i] = state;
    }

    // Normalize to prevent volume differences between soft/hard
    float maxAmp = 0.0f;
    for (int i = 0; i < v->ksLength; i++) {
        float a = fabsf(v->ksBuffer[i]);
        if (a > maxAmp) maxAmp = a;
    }
    if (maxAmp > 0.001f) {
        float scale = 1.0f / maxAmp;
        for (int i = 0; i < v->ksLength; i++) {
            v->ksBuffer[i] *= scale;
        }
    }
}

// Initialize allpass dispersion chain for string stiffness/inharmonicity
// stiffness: 0-1 user param, maps to inharmonicity coefficient B
// Higher B = more stretched upper partials (piano ~0.0001-0.01)
static void initStifKarpDispersion(StifKarpSettings *sk, float stiffness, float freq, float sampleRate) {
    // Quadratic mapping for musical control — low values have subtle effect
    float B = stiffness * stiffness * 0.015f;

    // Scale stiffness with frequency — treble strings are more inharmonic
    float freqScale = 1.0f + (freq - 261.0f) / 2000.0f;
    if (freqScale < 0.5f) freqScale = 0.5f;
    if (freqScale > 3.0f) freqScale = 3.0f;
    B *= freqScale;

    // Number of active dispersion stages (more = more accurate but heavier)
    sk->dispStages = (stiffness < 0.1f) ? 1 :
                     (stiffness < 0.4f) ? 2 :
                     (stiffness < 0.7f) ? 3 : STIFKARP_DISPERSION_STAGES;

    // Compute allpass coefficients
    // Each stage adds phase shift that stretches upper partials
    for (int i = 0; i < STIFKARP_DISPERSION_STAGES; i++) {
        if (i < sk->dispStages) {
            // Target phase shift increases per stage
            float phaseTarget = B * (float)(i + 1) * freq / sampleRate;
            if (phaseTarget > 0.9f) phaseTarget = 0.9f;
            sk->dispCoeffs[i] = (1.0f - phaseTarget) / (1.0f + phaseTarget);
        } else {
            sk->dispCoeffs[i] = 0.0f;
        }
        sk->dispStates[i] = 0.0f;
    }
}

// Initialize body resonator for a given stiff string preset
static void initStifKarpPreset(StifKarpSettings *sk, StifKarpPreset preset, float freq, float sampleRate) {
    sk->preset = preset;
    sk->loopFilterState = 0.0f;
    sk->buzzAmount = 0.0f;

    float formants[STIFKARP_BODY_MODES];
    float bandwidths[STIFKARP_BODY_MODES];
    float gains[STIFKARP_BODY_MODES];

    (void)freq;  // Body formants are fixed, not pitch-dependent

    switch (preset) {
        default:
        case STIFKARP_PIANO:
            // Grand piano soundboard: spruce top, strong low resonance
            formants[0] = 65.0f;   bandwidths[0] = 1.0f;  gains[0] = 1.0f;
            formants[1] = 130.0f;  bandwidths[1] = 0.8f;  gains[1] = 0.7f;
            formants[2] = 260.0f;  bandwidths[2] = 0.6f;  gains[2] = 0.4f;
            formants[3] = 520.0f;  bandwidths[3] = 0.5f;  gains[3] = 0.2f;
            sk->loopFilterCoeff = 0.18f;  // Warm, felt-damped — strong LP
            sk->hammerHardness = 0.45f;
            sk->strikePosition = 0.12f;
            break;

        case STIFKARP_BRIGHT_PIANO:
            // Concert grand: harder hammers, more overtones, longer soundboard
            formants[0] = 60.0f;   bandwidths[0] = 1.0f;  gains[0] = 1.0f;
            formants[1] = 125.0f;  bandwidths[1] = 0.8f;  gains[1] = 0.8f;
            formants[2] = 280.0f;  bandwidths[2] = 0.6f;  gains[2] = 0.5f;
            formants[3] = 600.0f;  bandwidths[3] = 0.5f;  gains[3] = 0.35f;
            sk->loopFilterCoeff = 0.3f;   // Brighter than regular piano
            sk->hammerHardness = 0.65f;
            sk->strikePosition = 0.11f;
            break;

        case STIFKARP_HARPSICHORD:
            // Smaller case, bright plectrum, sharp attack, fast decay
            formants[0] = 200.0f;  bandwidths[0] = 0.5f;  gains[0] = 0.8f;
            formants[1] = 400.0f;  bandwidths[1] = 0.4f;  gains[1] = 0.6f;
            formants[2] = 900.0f;  bandwidths[2] = 0.3f;  gains[2] = 0.4f;
            formants[3] = 1800.0f; bandwidths[3] = 0.3f;  gains[3] = 0.2f;
            sk->loopFilterCoeff = 0.7f;   // Very bright — metal plectrum snap
            sk->hammerHardness = 0.9f;
            sk->strikePosition = 0.08f;   // Near bridge (plectrum)
            break;

        case STIFKARP_DULCIMER:
            // Trapezoidal body, hammered wire strings, metallic shimmer
            formants[0] = 180.0f;  bandwidths[0] = 0.6f;  gains[0] = 1.0f;
            formants[1] = 360.0f;  bandwidths[1] = 0.5f;  gains[1] = 0.7f;
            formants[2] = 720.0f;  bandwidths[2] = 0.4f;  gains[2] = 0.45f;
            formants[3] = 1500.0f; bandwidths[3] = 0.3f;  gains[3] = 0.25f;
            sk->loopFilterCoeff = 0.5f;   // Medium bright — wire strings
            sk->hammerHardness = 0.7f;
            sk->strikePosition = 0.15f;
            break;

        case STIFKARP_CLAVICHORD:
            // Minimal body, tangent strike (very soft), intimate sound
            formants[0] = 150.0f;  bandwidths[0] = 0.8f;  gains[0] = 0.5f;
            formants[1] = 300.0f;  bandwidths[1] = 0.6f;  gains[1] = 0.3f;
            formants[2] = 600.0f;  bandwidths[2] = 0.4f;  gains[2] = 0.15f;
            formants[3] = 1200.0f; bandwidths[3] = 0.3f;  gains[3] = 0.08f;
            sk->loopFilterCoeff = 0.1f;   // Very dark — muffled, intimate
            sk->hammerHardness = 0.2f;
            sk->strikePosition = 0.5f;    // Tangent strikes at center
            break;

        case STIFKARP_PREPARED:
            // Piano with objects on strings — irregular buzz, muted partials
            formants[0] = 65.0f;   bandwidths[0] = 1.0f;  gains[0] = 0.9f;
            formants[1] = 140.0f;  bandwidths[1] = 0.7f;  gains[1] = 0.6f;
            formants[2] = 280.0f;  bandwidths[2] = 0.5f;  gains[2] = 0.3f;
            formants[3] = 550.0f;  bandwidths[3] = 0.4f;  gains[3] = 0.15f;
            sk->loopFilterCoeff = 0.15f;  // Dark — objects damping the string
            sk->hammerHardness = 0.5f;
            sk->strikePosition = 0.2f;
            sk->buzzAmount = 0.4f;        // Bolts rattling on strings
            break;

        case STIFKARP_HONKYTONK:
            // Detuned upright — uses unison for beating, slightly muffled
            formants[0] = 70.0f;   bandwidths[0] = 0.9f;  gains[0] = 0.9f;
            formants[1] = 145.0f;  bandwidths[1] = 0.7f;  gains[1] = 0.6f;
            formants[2] = 300.0f;  bandwidths[2] = 0.5f;  gains[2] = 0.35f;
            formants[3] = 600.0f;  bandwidths[3] = 0.4f;  gains[3] = 0.2f;
            sk->loopFilterCoeff = 0.2f;   // Muffled — old worn felt hammers
            sk->hammerHardness = 0.55f;
            sk->strikePosition = 0.13f;
            break;

        case STIFKARP_CELESTA:
            // Metal plates + wooden resonator, bell-like, high stiffness
            formants[0] = 400.0f;  bandwidths[0] = 0.4f;  gains[0] = 0.8f;
            formants[1] = 800.0f;  bandwidths[1] = 0.3f;  gains[1] = 0.6f;
            formants[2] = 1600.0f; bandwidths[2] = 0.3f;  gains[2] = 0.4f;
            formants[3] = 3200.0f; bandwidths[3] = 0.2f;  gains[3] = 0.2f;
            sk->loopFilterCoeff = 0.6f;   // Bright — metal plates ring clearly
            sk->hammerHardness = 0.35f;
            sk->strikePosition = 0.25f;
            break;
    }

    // Initialize body biquads
    for (int i = 0; i < STIFKARP_BODY_MODES; i++) {
        // Scale gains by body brightness
        float g = gains[i] * (0.5f + sk->bodyBrightness * 0.5f);
        initBodyBiquad(&sk->body[i], formants[i], bandwidths[i], g, sampleRate);
    }
}

// Process stiff string oscillator: KS + dispersion chain + body resonator
static float processStifKarpOscillator(Voice *v, float sampleRate) {
    if (v->ksLength <= 0) return 0.0f;
    StifKarpSettings *sk = &v->stifkarpSettings;

    // Step 1: Read from delay line
    float stringSample = v->ksBuffer[v->ksIndex];
    int nextIndex = (v->ksIndex + 1) % v->ksLength;
    float nextSample = v->ksBuffer[nextIndex];

    // Step 2: Loop filter (brightness decay — higher partials lose energy faster)
    // Damper modulates the effective damping: at full damper (1.0) = normal sustain,
    // at zero damper = increased energy loss per sample (faster decay)
    float avg = (stringSample + nextSample) * 0.5f;
    float bright = v->ksBrightness;
    float blended = avg + (stringSample - avg) * bright;
    float damp = pluckDamp;
    float filtered = v->ksLastSample + (blended - v->ksLastSample) * (1.0f - damp * 0.8f);
    // Damper: smoothly ramp toward target gain
    sk->damperState += (sk->damperGain - sk->damperState) * 0.0005f;
    // At damperState=1.0: effectiveDamping = ksDamping (normal)
    // At damperState=0.0: effectiveDamping = ksDamping * 0.992 (faster decay)
    float effectiveDamping = v->ksDamping * (0.992f + 0.008f * sk->damperState);
    filtered *= effectiveDamping;
    v->ksLastSample = filtered;

    // Step 3: Dispersion — cascade of allpass filters (the stiff-string magic)
    // Each allpass adds frequency-dependent phase shift, stretching upper partials sharp
    float dispersed = filtered;
    for (int i = 0; i < sk->dispStages; i++) {
        float C = sk->dispCoeffs[i];
        float apOut = C * dispersed + sk->dispStates[i];
        sk->dispStates[i] = dispersed - C * apOut;
        dispersed = apOut;
    }

    // Step 4: Fractional delay allpass (pitch tuning)
    float apOut = v->ksAllpassCoeff * dispersed + v->ksAllpassState;
    v->ksAllpassState = dispersed - v->ksAllpassCoeff * apOut;

    // Step 5: Write back to delay line
    v->ksBuffer[v->ksIndex] = apOut;
    v->ksIndex = nextIndex;

    // Step 6: Prepared piano buzz (optional — objects rattling on strings)
    float out = stringSample;
    if (sk->buzzAmount > 0.001f) {
        float threshold = 1.0f - sk->buzzAmount * 0.7f;
        if (fabsf(out) > threshold) {
            float excess = fabsf(out) - threshold;
            float sign = (out >= 0.0f) ? 1.0f : -1.0f;
            out = sign * (threshold + tanhf(excess * 4.0f) * (1.0f - threshold));
        }
        // Feed buzz energy back for sustained rattling
        v->ksBuffer[(v->ksIndex + v->ksLength / 2) % v->ksLength] +=
            (out - stringSample) * sk->buzzAmount * 0.08f;
    }

    // Step 7: Output tone filter — per-preset character shaping
    // loopFilterCoeff: low = dark/muffled (clavichord 0.3), high = bright/jangly (harpsichord 0.55)
    // 1-pole LP on the output: coeff controls how much high-frequency passes through
    sk->loopFilterState += sk->loopFilterCoeff * (out - sk->loopFilterState);
    out = sk->loopFilterState;

    // Step 8: Body resonator (soundboard/case)
    float bodyOut = 0.0f;
    for (int i = 0; i < STIFKARP_BODY_MODES; i++) {
        bodyOut += processBodyBiquad(&sk->body[i], out);
    }
    bodyOut *= 0.35f;

    // Step 9: Mix dry string + wet body — proper crossfade
    // At bodyMix=0: pure filtered string. At bodyMix=1: full body resonance.
    out = out * (1.0f - sk->bodyMix * 0.6f) + bodyOut * sk->bodyMix;

    // Step 10: Sympathetic resonance — feed tiny amount back at 3rd partial
    if (sk->sympatheticLevel > 0.001f) {
        int sympTap = v->ksLength / 3;
        if (sympTap > 0) {
            v->ksBuffer[(v->ksIndex + sympTap) % v->ksLength] +=
                out * sk->sympatheticLevel * 0.015f;
        }
    }

    (void)sampleRate;
    return out;
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
// Uses allpass fractional delay for accurate tuning (Jaffe & Smith 1983)
static void initPluck(Voice *v, float frequency, float sampleRate, float brightness, float damping) {
    // Calculate delay length from frequency — use fractional part for allpass tuning
    float idealLength = sampleRate / frequency;
    v->ksLength = (int)idealLength;
    if (v->ksLength > 2047) v->ksLength = 2047;
    if (v->ksLength < 2) v->ksLength = 2;

    // First-order allpass for fractional delay: corrects pitch to exact frequency
    // Without this, high notes are audibly out of tune (up to 39 cents at 4kHz)
    float frac = idealLength - (float)v->ksLength;
    // Thiran allpass coefficient: C = (1 - frac) / (1 + frac)
    // At frac=0: C=1 (no delay), at frac=0.5: C=0.33, at frac→1: C→0
    v->ksAllpassCoeff = (1.0f - frac) / (1.0f + frac);
    v->ksAllpassState = 0.0f;

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

// Initialize reed instrument waveguide
// The delay line represents the full round-trip through the bore.
// For cylindrical bore (clarinet), the closed reed end reflects with same sign,
// creating odd-harmonic series. For conical bore (sax), the open end reflects
// with inverted sign, creating all harmonics.
static void initReed(Voice *v, float frequency, float sampleRate) {
    ReedSettings *rs = &v->reedSettings;

    // Bore delay = half-wavelength (one-way trip through the bore).
    // The -1 reflection at the open end provides the return trip,
    // so total round trip = 2 * boreLen samples = one full period.
    int boreLen = (int)(sampleRate / frequency / 2.0f);
    if (boreLen > 1023) boreLen = 1023;
    if (boreLen < 4) boreLen = 4;

    rs->boreLen = boreLen;
    rs->boreIdx = 0;

    // Seed bore with tiny noise (faster startup than silence)
    for (int i = 0; i < rs->boreLen; i++) {
        rs->boreBuf[i] = noise() * 0.01f;
    }

    rs->blowPressure = reedBlowPressure;
    rs->stiffness = reedStiffness;
    rs->aperture = reedAperture;
    rs->bore = reedBore;
    rs->vibratoDepth = reedVibratoDepth;

    // Reed starts at rest position (aperture opening)
    rs->reedX = reedAperture;
    rs->reedV = 0.0f;
    rs->lpState = 0.0f;
    rs->dcState = 0.0f;
    rs->dcPrev = 0.0f;
    rs->vibratoPhase = 0.0f;
}

// Initialize brass instrument waveguide
// The lip oscillator frequency is set slightly above the bore fundamental
// to encourage mode-locking (the lip will pull down to the bore resonance).
static void initBrass(Voice *v, float frequency, float sampleRate) {
    BrassSettings *bs = &v->brassSettings;

    // Bore delay = half-wavelength (same as reed — round trip with bell reflection)
    int boreLen = (int)(sampleRate / frequency / 2.0f);
    if (boreLen > 1023) boreLen = 1023;
    if (boreLen < 4) boreLen = 4;

    bs->boreLen = boreLen;
    bs->boreIdx = 0;

    // Seed bore with tiny noise (helps oscillation startup)
    for (int i = 0; i < bs->boreLen; i++) {
        bs->boreBuf[i] = noise() * 0.005f;
    }

    bs->blowPressure = brassBlowPressure;
    bs->lipTension = brassLipTension;
    bs->lipAperture = brassLipAperture;
    bs->bore = brassBore;
    bs->mute = brassMute;

    // Lip resonant frequency: set slightly above the target pitch
    // The lip will lock to the nearest bore mode during oscillation
    // Higher lip tension → higher frequency (tighter embouchure)
    bs->lipFreq = frequency * (1.0f + brassLipTension * 0.15f);

    // Lip starts at rest (equilibrium opening)
    bs->lipX = brassLipAperture * 0.5f;
    bs->lipV = 0.0f;
    bs->lpState = 0.0f;
    bs->lpState2 = 0.0f;
    bs->dcState = 0.0f;
    bs->dcPrev = 0.0f;
}

// ============================================================================
// ELECTRIC PIANO (RHODES) — Tine modal bank + electromagnetic pickup nonlinearity
// ============================================================================
// Physics: The Rhodes is a tuning fork — hammer strikes a tine (cantilever beam)
// coupled to a tone bar, vibrating near an electromagnetic pickup.
//
// Two phenomena create the sound:
// 1. Attack transient: cantilever beam bending modes (wildly inharmonic: 1×, 6.27×,
//    17.55×...) from Euler-Bernoulli beam equation. These decay in ~5-20ms but give
//    the characteristic bell-like "chime" attack.
// 2. Sustained tone: the tine settles into simple harmonic motion, and the nonlinear
//    electromagnetic pickup generates integer harmonics (2f, 3f, 4f...) via its
//    transfer function. This is what you hear after the attack.
//
// Our model blends between these via epBellTone: 0 = pure pickup harmonics (sustained),
// 1 = cantilever beam modes (bell attack). The fast decay of upper modes naturally
// handles the transient-vs-sustain split.
//
// References:
// - Shear & Wright, "The Electromagnetically Sustained Rhodes Piano" (UCSB, 2011)
// - Pfeifle, "Real-time Physical Model of a Rhodes" (DAFx-17, 2017)
// - Gabrielli et al., "The Rhodes electric piano" (JASA 2020) — laser vibrometry
// - Fletcher & Rossing, "The Physics of Musical Instruments" (1998)

// Initialize EPiano settings at note-on.
// Configures 6 mode amplitudes based on frequency register, pickup position,
// hammer hardness, and velocity. Supports Rhodes (electromagnetic), Wurlitzer
// (electrostatic), and Clavinet (contact) pickup types.
static void initEPianoSettings(EPianoSettings *ep, float freq, float velocity) {
    float vel = clampf(velocity * 2.0f, 0.0f, 1.0f); // normalize from volume (0-0.5 typical) to 0-1
    int pickupT = epPickupType;
    ep->pickupType = pickupT;

    // Register variation: timbre changes across the keyboard for all instruments.
    // Rhodes: Low=warm/piano, Mid=classic, High=bell/thin
    // Wurli:  Low=boxy/reedy/organ, Mid=classic bark, High=thin/buzzy
    // Clav:   Low=thick/thumpy, Mid=funky, High=thin/percussive/clicky
    float freqNorm = clampf((freq - 80.0f) / 1200.0f, 0.0f, 1.0f);

    // Mode frequency ratios: blend between harmonic and instrument-specific inharmonic modes.
    static const float harmonicRatios[EPIANO_MODES] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    // Rhodes: clamped-free cantilever beam bending modes (Euler-Bernoulli).
    static const float rhodesInharmonic[EPIANO_MODES] = {1.0f, 6.27f, 17.55f, 34.39f, 56.84f, 84.91f};
    // Wurlitzer: clamped-free reed modes — closer to odd harmonics than beam modes.
    // Steel reeds vibrate with a strong odd-harmonic series (like a clarinet-ish spectrum)
    // but with slight inharmonic stretch from stiffness. The 2nd partial is weak because
    // the electrostatic pickup's symmetric geometry cancels even modes partially.
    static const float wurliInharmonic[EPIANO_MODES] = {1.0f, 2.02f, 3.01f, 5.04f, 7.05f, 9.08f};
    // Clavinet D6: short steel string under tension with yarn-wound damper pad.
    // Nearly harmonic (stiff string, not a beam), with slight inharmonicity from
    // string stiffness (B coefficient). Upper modes include damper pad resonance —
    // the characteristic "twang" comes from the pad releasing the string.
    // Mode 5-6: damper/body resonance (more inharmonic, fast-decaying).
    static const float clavinetInharmonic[EPIANO_MODES] = {1.0f, 2.003f, 3.012f, 4.028f, 5.15f, 6.35f};

    const float *inharmonicRatios =
        (pickupT == EP_PICKUP_CONTACT) ? clavinetInharmonic :
        (pickupT == EP_PICKUP_ELECTROSTATIC) ? wurliInharmonic : rhodesInharmonic;

    float bt = epBellTone;
    float regBellBoost = freqNorm * 0.15f;
    float btEff = clampf(bt + regBellBoost, 0.0f, 1.0f);
    // Per-mode blend: body modes stay harmonic, upper modes get inharmonicity.
    static const float modeBlendScale[EPIANO_MODES] = {0.0f, 0.0f, 0.05f, 0.4f, 0.8f, 1.0f};
    // Wurli reed modes are already close to harmonic, so blend more aggressively
    static const float wurliBlendScale[EPIANO_MODES] = {0.0f, 0.1f, 0.2f, 0.6f, 0.9f, 1.0f};
    // Clavinet string modes are nearly harmonic — only upper modes (damper pad)
    // get significant inharmonicity. Body modes stay locked to partials.
    static const float clavinetBlendScale[EPIANO_MODES] = {0.0f, 0.0f, 0.05f, 0.2f, 0.5f, 0.75f};
    const float *blendScale =
        (pickupT == EP_PICKUP_CONTACT) ? clavinetBlendScale :
        (pickupT == EP_PICKUP_ELECTROSTATIC) ? wurliBlendScale : modeBlendScale;
    float modeRatios[EPIANO_MODES];
    for (int i = 0; i < EPIANO_MODES; i++) {
        float modeBt = btEff * blendScale[i];
        modeRatios[i] = harmonicRatios[i] * (1.0f - modeBt) + inharmonicRatios[i] * modeBt;
    }

    // Pickup position profiles: centered (mellow) vs offset (bright).
    // Rhodes (electromagnetic): models magnetic flux coupling between tine and pickup coil.
    // Centered: fundamental + strong 2nd partial. The electromagnetic pickup always couples
    // to the 2nd partial — it's what gives Rhodes its warmth vs a hollow organ sound.
    // Offset: 2nd harmonic dominant (the "bark" position), 4th partial also strong.
    // Measured Rhodes spectra (Gabrielli 2020, Shear 2011) show the 2nd partial at
    // 60-80% of fundamental at centered position, rising to equal or greater at offset.
    static const float rhodesCentered[EPIANO_MODES] = {1.00f, 0.55f, 0.12f, 0.18f, 0.08f, 0.03f};
    static const float rhodesOffset[EPIANO_MODES]   = {0.40f, 1.00f, 0.20f, 0.50f, 0.15f, 0.06f};
    // Wurli (electrostatic): reed vibrates between charged plates. More uniform coupling
    // but the reed's own mode shapes favor odd harmonics. Centered = clean, offset = buzzy.
    static const float wurliCentered[EPIANO_MODES]   = {1.00f, 0.10f, 0.55f, 0.08f, 0.35f, 0.05f};
    static const float wurliOffset[EPIANO_MODES]     = {0.60f, 0.20f, 0.70f, 0.15f, 0.50f, 0.10f};
    // Clavinet D6: two single-coil pickups under the strings (like a guitar).
    // "Neck" position (centered) = warm, fundamental-heavy, damper pad resonance audible.
    // "Bridge" position (offset) = bright, snappy, strong upper partials, the funk sound.
    // Real D6 has a 4-position pickup selector; we interpolate between the two extremes.
    static const float clavinetCentered[EPIANO_MODES] = {1.00f, 0.30f, 0.20f, 0.35f, 0.15f, 0.06f};
    static const float clavinetOffset[EPIANO_MODES]   = {0.60f, 0.55f, 0.50f, 0.20f, 0.30f, 0.10f};
    const float *centeredAmps =
        (pickupT == EP_PICKUP_CONTACT) ? clavinetCentered :
        (pickupT == EP_PICKUP_ELECTROSTATIC) ? wurliCentered : rhodesCentered;
    const float *offsetAmps =
        (pickupT == EP_PICKUP_CONTACT) ? clavinetOffset :
        (pickupT == EP_PICKUP_ELECTROSTATIC) ? wurliOffset : rhodesOffset;
    float pos = epPickupPos;

    // Velocity-dependent hammer hardness: real neoprene tips compress more at higher
    // velocity, effectively getting harder. Soft hits = mellow spectrum, hard hits = bright.
    float hard = epHardness + vel * (1.0f - epHardness) * 0.3f;
    float velSq = vel * vel; // squared velocity curve

    for (int i = 0; i < EPIANO_MODES; i++) {
        ep->modeRatios[i] = modeRatios[i];

        // Blend pickup position profiles
        float amp = centeredAmps[i] * (1.0f - pos) + offsetAmps[i] * pos;

        // Hammer hardness: spectral tilt — soft hammer attenuates upper modes more
        float ratio = modeRatios[i];
        amp *= hard + (1.0f - hard) / (1.0f + (ratio - 1.0f) * 0.5f);

        // Bell emphasis for modes 4-6
        if (i >= 3) amp *= (1.0f + epBell * 1.5f);

        // Register scaling: high notes have less body, more bell
        if (i == 0) {
            amp *= (1.0f - freqNorm * 0.3f); // fundamental thins at top
        } else if (i >= 3) {
            amp *= (1.0f + freqNorm * 0.4f); // bell modes brighter at top
        }

        // Velocity scaling: controls which modes are heard at different dynamics.
        // Fundamental/body use vel² (only loud at hard hits).
        // Bell modes use sqrt(vel) (present even at soft hits → vibes character).
        // The actual bark comes from velToDrive on the preset, not from here.
        float velScale;
        if (i == 0) {
            // Fundamental: vel² curve — quiet at pp, strong at ff
            float fundPower = velSq * (1.0f - epBell * 0.5f) + vel * epBell * 0.5f;
            velScale = 0.05f + 0.95f * fundPower;
        } else if (i == 1) {
            // 2nd partial: the key even harmonic for Rhodes warmth. Must track the
            // fundamental more closely than other body modes — if it disappears at
            // soft dynamics the sound becomes hollow/organ-like. Use linear curve
            // (between fundamental and body) so it's always audible relative to fund.
            velScale = 0.08f + 0.92f * (vel * 0.6f + velSq * 0.4f);
        } else if (i == 2) {
            // 3rd partial (body): vel² dominant
            velScale = 0.05f + 0.95f * (vel * 0.4f + velSq * 0.6f);
        } else {
            // Bell modes: sqrt curve — prominent even at soft velocity
            float bellCurve = sqrtf(vel);
            float mix = 0.3f + epBell * 0.4f; // how much sqrt vs linear (0.3-0.7)
            velScale = 0.05f + 0.95f * (bellCurve * mix + vel * (1.0f - mix));
        }
        amp *= velScale;

        ep->modeAmpsInit[i] = amp;
        ep->modeAmps[i] = amp;
        ep->modePhases[i] = 0.0f;

        // Decay: upper modes decay much faster than fundamental.
        // For cantilever beam modes, higher modes decay roughly as 1/ratio² due to
        // radiation loss and internal damping (Shear 2011: beam modes decay in ms).
        // For pickup harmonics, upper harmonics decay faster but less drastically.
        // The blend follows bellTone: more inharmonic = faster upper decay.
        float decayFactor = 1.0f / (1.0f + (ratio - 1.0f) * (0.3f + btEff * 0.7f));
        // Register: high notes have shorter decay (measured: Eb6=0.45s vs Eb2=3.88s)
        float regDecay = epDecay * (1.0f - freqNorm * 0.5f);
        ep->modeDecays[i] = regDecay * decayFactor;
    }

    // Normalize mode amplitudes so peak sum ≈ vel (soft=small signal, hard=big signal)
    // This is critical: the pickup nonlinearity is amplitude-dependent,
    // so soft hits must produce a small clean signal, hard hits drive it into bark
    float ampSum = 0.0f;
    for (int i = 0; i < EPIANO_MODES; i++) ampSum += ep->modeAmps[i];
    if (ampSum > 0.001f) {
        float target = vel;  // signal amplitude tracks velocity
        float scale = target / ampSum;
        for (int i = 0; i < EPIANO_MODES; i++) {
            ep->modeAmps[i] *= scale;
            ep->modeAmpsInit[i] *= scale;
        }
    }

    // Tone bar coupling (Rhodes only): extends decay of fundamental AND 2nd partial.
    // The tone bar is a resonant body tuned to reinforce the fundamental.
    // Wurlitzer and Clavinet have no tone bar — decay naturally without reinforcement.
    if (pickupT == EP_PICKUP_ELECTROMAGNETIC) {
        ep->modeDecays[0] *= (1.0f + epToneBar * 1.5f);
        ep->modeDecays[1] *= (1.0f + epToneBar * 0.6f);
    }

    ep->hardness = hard;
    ep->toneBarCoupling = epToneBar;
    ep->pickupPos = pos;
    ep->pickupDist = epPickupDist;
    ep->decayTime = epDecay;
    ep->bellLevel = epBell;
    ep->strikeVelocity = vel;
    ep->dcBlockState = 0.0f;
    ep->dcBlockPrev = 0.0f;
}

// Process one sample of the electric piano oscillator.
// Sums 6 harmonic sine modes with independent exponential decay,
// then applies pickup nonlinearity (Rhodes/Wurli/Clavinet).
static float processEPianoOscillator(Voice *v, float sampleRate) {
    EPianoSettings *ep = &v->epianoSettings;
    float dt = 1.0f / sampleRate;
    float sum = 0.0f;

    for (int i = 0; i < EPIANO_MODES; i++) {
        if (ep->modeAmps[i] < 0.0001f) continue;

        // Advance phase for this mode (blend of pickup harmonics + cantilever beam modes)
        float modeFreq = v->frequency * ep->modeRatios[i];
        ep->modePhases[i] += modeFreq * dt;
        // fmodf avoids precision loss on long sustains (float mantissa = 23 bits,
        // at 2kHz after 4s = 8000 cycles, floorf approach loses ~0.001 precision)
        ep->modePhases[i] = fmodf(ep->modePhases[i], 1.0f);

        // Sine oscillator for this mode
        float modeSample = sinf(ep->modePhases[i] * 2.0f * PI) * ep->modeAmps[i];
        sum += modeSample;

        // Exponential decay
        if (ep->modeDecays[i] > 0.0f) {
            ep->modeAmps[i] *= 1.0f - dt / ep->modeDecays[i];
            if (ep->modeAmps[i] < 0.0001f) ep->modeAmps[i] = 0.0f;
        }
    }

    // Pickup nonlinearity — differs by pickup type:
    // Rhodes (electromagnetic): asymmetric sum² (even harmonics = bark) + sum³
    // Wurli (electrostatic): symmetric sum³ + sum⁵ (odd harmonics = reedy/nasal buzz)
    // Clavinet (contact): mixed even+odd (sum² + sum³), symmetric clip — funky twang
    float velBoost = 1.0f + ep->strikeVelocity * ep->pickupDist;
    float output;
    if (ep->pickupType == EP_PICKUP_CONTACT) {
        // Contact/magnetic pickup: string plucked by tangent, two single-coil pickups.
        // The Clavinet's character is brighter and more aggressive than Rhodes —
        // the short string produces a rich harmonic series, and the pickup sees both
        // even and odd harmonics. sum² gives the funky "honk", sum³ adds bite.
        float k2 = ep->pickupDist * 0.8f * velBoost;   // even harmonics (honk/wah)
        float k3 = ep->pickupDist * 1.0f * velBoost;   // odd harmonics (bite/growl)
        float sum2 = sum * sum;
        output = sum + k2 * sum2 + k3 * sum * sum2;
        // Symmetric soft-clip — the Clavinet's pickup response is more symmetric
        // than Rhodes (no tine-passing-coil asymmetry), but clips harder
        output = tanhf(output * 1.2f);
    } else if (ep->pickupType == EP_PICKUP_ELECTROSTATIC) {
        // Electrostatic pickup: reed between two charged plates — symmetric response.
        // Odd-order terms only (sum³, sum⁵) preserve the reed's odd-harmonic character.
        float k3 = ep->pickupDist * 1.5f * velBoost;
        float k5 = ep->pickupDist * 0.4f * velBoost;
        float sum2 = sum * sum;
        output = sum + k3 * sum * sum2 + k5 * sum * sum2 * sum2;
        // Symmetric soft-clip (preserves odd harmonics)
        output = tanhf(output);
    } else {
        // Electromagnetic pickup: tine magnetizes asymmetrically.
        // Real Rhodes pickups have a baseline asymmetry even at soft dynamics —
        // the tine's proximity to the coil creates a nonlinear flux coupling that
        // always generates some even harmonics. This isn't an on/off effect.
        // kBase provides a floor so soft hits still have warmth (not just odd partials).
        // The sum² term (k) is the primary even-harmonic generator; it needs a strong
        // baseline to avoid the hollow/square-wave character that pure modal sines produce.
        // kBase is pickup-distance-independent — the asymmetry is inherent in the geometry.
        float kBase = 0.4f;  // always-on even harmonics from coil asymmetry
        float k = kBase + ep->pickupDist * 1.0f * velBoost;
        float k2 = 0.12f + ep->pickupDist * 0.25f * velBoost;
        output = sum + k * sum * sum + k2 * sum * sum * sum;
        // Asymmetric soft-clip (positive peaks compress less = even harmonic preservation)
        if (output >= 0.0f) {
            output = tanhf(output);
        } else {
            output = tanhf(output * 0.85f) * 0.9f;
        }
    }

    // DC blocker (pickup AC coupling) — removes DC from sum² term
    // y[n] = x[n] - x[n-1] + R * y[n-1], R = 0.995 (~7 Hz cutoff at 44.1k)
    float dcIn = output;
    output = dcIn - ep->dcBlockPrev + 0.995f * ep->dcBlockState;
    ep->dcBlockPrev = dcIn;
    ep->dcBlockState = output;

    return output;
}

// ============================================================================
// ORGAN (HAMMOND DRAWBAR) SYNTHESIS
// ============================================================================
// 9-drawbar tonewheel organ with key click, single-trigger percussion,
// and tonewheel crosstalk. Drawbar ratios from Hammond B3:
//   16'=0.5  5⅓'=1.5  8'=1.0  4'=2.0  2⅔'=3.0  2'=4.0  1⅗'=5.0  1⅓'=6.0  1'=8.0

static const float organDrawbarRatios[ORGAN_DRAWBARS] = {
    0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f
};

static void initOrganSettings(OrganSettings *os, float freq, float velocity) {
    (void)freq;
    float vel = clampf(velocity * 2.0f, 0.0f, 1.0f);

    // Cache drawbar levels from globals
    for (int i = 0; i < ORGAN_DRAWBARS; i++) {
        os->drawbarLevels[i] = orgDrawbars[i];
        os->drawbarPhases[i] = 0.0f;
    }

    // Key click: velocity-scaled noise burst
    os->clickEnv = orgClick * vel;

    // Percussion: single-trigger — only fires if no other organ keys are held
    if (orgPercOn && orgPercKeysHeld == 0) {
        os->percEnv = 1.0f;
    } else {
        os->percEnv = 0.0f;
    }
    os->percPhase = 0.0f;
    orgPercKeysHeld++;

    // Scanner vibrato: clear delay buffer
    for (int i = 0; i < ORGAN_VIBRATO_BUFSIZE; i++) os->vibratoBuffer[i] = 0.0f;
    os->vibratoWritePos = 0;
    os->scannerPhase = 0.0f;
}

static float processOrganOscillator(Voice *v, float sampleRate) {
    OrganSettings *os = &v->organSettings;
    float dt = 1.0f / sampleRate;
    float sum = 0.0f;
    float ampSum = 0.0f;

    // Sum 9 drawbar sine oscillators
    for (int i = 0; i < ORGAN_DRAWBARS; i++) {
        float level = os->drawbarLevels[i];
        if (level < 0.001f) continue;

        float drawbarFreq = v->frequency * organDrawbarRatios[i];

        // Skip if above Nyquist
        if (drawbarFreq >= sampleRate * 0.5f) continue;

        os->drawbarPhases[i] += drawbarFreq * dt;
        os->drawbarPhases[i] = fmodf(os->drawbarPhases[i], 1.0f);

        float s = sinf(os->drawbarPhases[i] * 2.0f * PI) * level;
        sum += s;
        ampSum += level;

        // Tonewheel crosstalk: subtle leakage from adjacent wheels (~-36dB)
        if (orgCrosstalk > 0.001f) {
            sum += sinf((os->drawbarPhases[i] + 0.037f) * 2.0f * PI)
                   * level * orgCrosstalk * 0.015f;
        }
    }

    // Normalize to prevent clipping when many drawbars are active
    if (ampSum > 1.0f) {
        sum /= ampSum;
    }

    // Key click: filtered noise burst with ~3ms exponential decay
    if (os->clickEnv > 0.0001f) {
        sum += noise() * os->clickEnv * 0.5f;
        os->clickEnv *= 1.0f - dt / 0.003f;
        if (os->clickEnv < 0.0001f) os->clickEnv = 0.0f;
    }

    // Percussion: 2nd or 3rd harmonic with fast decay (single-trigger)
    if (os->percEnv > 0.0001f) {
        float percRatio = (orgPercHarmonic == 1) ? 3.0f : 2.0f;
        os->percPhase += v->frequency * percRatio * dt;
        os->percPhase = fmodf(os->percPhase, 1.0f);
        float percLevel = orgPercSoft ? 0.5f : 1.0f;
        float percDecay = orgPercFast ? 0.2f : 0.5f;
        sum += sinf(os->percPhase * 2.0f * PI) * os->percEnv * percLevel * 0.5f;
        os->percEnv *= 1.0f - dt / percDecay;
        if (os->percEnv < 0.0001f) os->percEnv = 0.0f;
    }

    // Scanner vibrato/chorus (Hammond V/C system)
    // Delay-line scanner: 6.9 Hz sine LFO modulates read position in short buffer.
    // V modes = wet only (pitch vibrato), C modes = 50/50 dry+wet (chorus shimmer).
    if (orgVibratoMode > 0 && orgVibratoMode <= 6) {
        // Depth in samples: V1/C1=±0.15ms, V2/C2=±0.40ms, V3/C3=±0.70ms
        static const float depthSamples[] = {0.0f, 6.6f, 17.6f, 30.9f, 6.6f, 17.6f, 30.9f};
        float depth = depthSamples[orgVibratoMode];
        bool isChorus = (orgVibratoMode >= 4);

        // Write mixed drawbar sum into delay buffer
        os->vibratoBuffer[os->vibratoWritePos] = sum;

        // Scanner LFO: 6.9 Hz (locked to tonewheel motor, 60Hz/8.7 gear ratio)
        os->scannerPhase += 6.9f * dt;
        if (os->scannerPhase >= 1.0f) os->scannerPhase -= 1.0f;
        float lfo = sinf(os->scannerPhase * 2.0f * PI);

        // Modulated read from buffer center (32 samples ≈ 0.73ms)
        float readDelay = 32.0f + lfo * depth;
        float readPos = (float)os->vibratoWritePos - readDelay;
        if (readPos < 0.0f) readPos += ORGAN_VIBRATO_BUFSIZE;
        float wet = cubicHermite(os->vibratoBuffer, ORGAN_VIBRATO_BUFSIZE, readPos);

        os->vibratoWritePos = (os->vibratoWritePos + 1) % ORGAN_VIBRATO_BUFSIZE;

        sum = isChorus ? 0.5f * sum + 0.5f * wet : wet;
    }

    return sum;
}

// ============================================================================
// SHAKER PERCUSSION (PhISM particle collision model — Cook 1997)
// ============================================================================

// Per-voice noise for shaker (avoids correlation between simultaneous voices)
static inline float shakerNoise(unsigned int *state) {
    *state = *state * 1103515245u + 12345u;
    return (float)(*state >> 16) / 32768.0f - 1.0f;
}

// Random float 0-1 from shaker state
static inline float shakerRand01(unsigned int *state) {
    *state = *state * 1103515245u + 12345u;
    return (float)(*state >> 16) / 65536.0f;
}

// Resonator preset data: {freq, bandwidth, gain} per resonator per preset
typedef struct {
    float freq;
    float bw;
    float gain;
} ShakerResPreset;

static const ShakerResPreset shakerResData[SHAKER_COUNT][SHAKER_NUM_RESONATORS] = {
    // MARACA: gourd shell resonance, mid-bright
    {{ 3200.0f, 350.0f, 1.0f }, { 6500.0f, 500.0f, 0.6f },
     { 9800.0f, 700.0f, 0.3f }, { 1800.0f, 250.0f, 0.4f }},
    // CABASA: metal beads, bright and dense
    {{ 5000.0f, 400.0f, 1.0f }, { 8200.0f, 600.0f, 0.7f },
     { 11500.0f, 800.0f, 0.4f }, { 3500.0f, 300.0f, 0.5f }},
    // TAMBOURINE: jingles have tonal peaks
    {{ 2800.0f, 120.0f, 1.0f }, { 5600.0f, 150.0f, 0.8f },
     { 8400.0f, 200.0f, 0.5f }, { 11200.0f, 300.0f, 0.3f }},
    // SLEIGH BELLS: bright metal, tight resonance
    {{ 4500.0f, 100.0f, 1.0f }, { 6800.0f, 120.0f, 0.9f },
     { 9200.0f, 150.0f, 0.6f }, { 12000.0f, 200.0f, 0.4f }},
    // BAMBOO: lower-pitched, woody
    {{ 1200.0f, 200.0f, 1.0f }, { 2800.0f, 300.0f, 0.5f },
     { 4200.0f, 400.0f, 0.25f }, { 800.0f, 150.0f, 0.3f }},
    // RAIN STICK: wide spectrum, many frequencies
    {{ 1600.0f, 600.0f, 0.8f }, { 4000.0f, 800.0f, 0.6f },
     { 7500.0f, 1000.0f, 0.4f }, { 2400.0f, 500.0f, 0.5f }},
    // GUIRO: scraping resonance, focused
    {{ 2500.0f, 200.0f, 1.0f }, { 4000.0f, 250.0f, 0.7f },
     { 5800.0f, 300.0f, 0.4f }, { 1500.0f, 180.0f, 0.3f }},
    // SANDPAPER: dense, wide bandwidth noise
    {{ 4000.0f, 1200.0f, 0.8f }, { 8000.0f, 1500.0f, 0.6f },
     { 2000.0f, 800.0f, 0.5f }, { 12000.0f, 2000.0f, 0.3f }},
};

// Preset parameter table
typedef struct {
    int numParticles;
    float systemDecay;
    float collisionProb;
    float collisionGain;
    int numResonators;
    float scrapeRate;      // 0 = random, >0 = Hz for periodic
    float scrapeJitter;
    float soundLevel;
} ShakerPresetParams;

static const ShakerPresetParams shakerPresetData[SHAKER_COUNT] = {
    // MARACA: 20 particles, medium decay, classic
    { 20, 0.9994f, 0.04f,  0.7f,  4, 0.0f,   0.0f, 0.45f },
    // CABASA: 40 particles, fast decay, dense bright
    { 40, 0.9990f, 0.06f,  0.5f,  4, 0.0f,   0.0f, 0.40f },
    // TAMBOURINE: 24 particles, medium decay, tonal jingles
    { 24, 0.9993f, 0.035f, 0.65f, 4, 0.0f,   0.0f, 0.50f },
    // SLEIGH BELLS: 48 particles, long sustain, bright metal
    { 48, 0.9997f, 0.03f,  0.55f, 4, 0.0f,   0.0f, 0.40f },
    // BAMBOO: 10 particles, long ring, sparse
    { 10, 0.9996f, 0.025f, 0.9f,  3, 0.0f,   0.0f, 0.50f },
    // RAIN STICK: 64 particles, very slow decay, many tiny hits
    { 64, 0.9998f, 0.015f, 0.35f, 4, 0.0f,   0.0f, 0.35f },
    // GUIRO: 12 particles, periodic scraping
    { 12, 0.9992f, 0.05f,  0.8f,  3, 120.0f, 0.2f, 0.50f },
    // SANDPAPER: 50 particles, short decay, dense noise
    { 50, 0.9985f, 0.07f,  0.3f,  4, 0.0f,   0.0f, 0.35f },
};

// Initialize shaker preset from data tables
static void initShakerPreset(ShakerSettings *ss, ShakerPreset preset) {
    if (preset < 0 || preset >= SHAKER_COUNT) preset = SHAKER_MARACA;
    ss->preset = preset;

    const ShakerPresetParams *pp = &shakerPresetData[preset];
    const ShakerResPreset *rp = shakerResData[preset];

    ss->numParticles = pp->numParticles;
    ss->particleEnergy = 1.0f;   // Full energy at note-on
    ss->systemDecay = pp->systemDecay;
    ss->collisionProb = pp->collisionProb;
    ss->collisionGain = pp->collisionGain;
    ss->numResonators = pp->numResonators;
    ss->scrapeRate = pp->scrapeRate;
    ss->scrapePhase = 0.0f;
    ss->scrapeJitter = pp->scrapeJitter;
    ss->soundLevel = pp->soundLevel;

    // Seed per-voice noise (will be xored with voice index for uniqueness)
    ss->noiseState = 0xDEADBEEF;

    // Initialize resonators from preset data
    for (int i = 0; i < SHAKER_NUM_RESONATORS; i++) {
        ss->res[i].freq = rp[i].freq;
        ss->res[i].bw = rp[i].bw;
        ss->res[i].gain = rp[i].gain;
        ss->res[i].y1 = 0.0f;
        ss->res[i].y2 = 0.0f;
    }
}

// SVF bandpass resonator (same topology as existing formant filter)
static inline float processShakerResonator(ShakerResonator *r, float input, float sampleRate) {
    float fc = 2.0f * sinf(PI * r->freq / sampleRate);
    if (fc > 0.99f) fc = 0.99f;
    float q = r->freq / (r->bw + 1.0f);
    if (q < 0.5f) q = 0.5f;
    if (q > 30.0f) q = 30.0f;

    r->y1 += fc * r->y2;                    // lowpass
    float hp = input - r->y1 - r->y2 / q;   // highpass
    r->y2 += fc * hp;                        // bandpass

    return r->y2 * r->gain;
}

// Main shaker oscillator — PhISM particle collision model
static float processShakerOscillator(Voice *v, float sampleRate) {
    ShakerSettings *ss = &v->shakerSettings;
    float dt = 1.0f / sampleRate;

    // Decay shaking energy
    ss->particleEnergy *= ss->systemDecay;

    // While key is held (attack/decay/sustain), maintain energy floor
    // so shaker keeps producing sound — like continuously shaking
    if (v->envStage >= 1 && v->envStage <= 3) {
        if (ss->particleEnergy < 0.3f) ss->particleEnergy = 0.3f;
    }

    // Re-boost energy on retrigger (envelope restart = new shake burst)
    if (v->envStage == 1 && v->envPhase < 0.002f) {
        ss->particleEnergy = 1.0f;
    }

    if (ss->particleEnergy < 0.0001f) return 0.0f;  // Silent, skip processing

    // Calculate collision impulse for this sample
    float impulse = 0.0f;

    if (ss->scrapeRate > 0.0f) {
        // GUIRO MODE: periodic collisions with jitter
        ss->scrapePhase += ss->scrapeRate * dt;
        if (ss->scrapePhase >= 1.0f) {
            ss->scrapePhase -= 1.0f;
            float jitterOffset = ss->scrapeJitter * (shakerNoise(&ss->noiseState) * 0.5f);
            impulse = ss->collisionGain * ss->particleEnergy * (1.0f + jitterOffset);
        }
    } else {
        // RANDOM COLLISION MODE (PhISM)
        // Expected collisions per sample (binomial approximation)
        float expectedCollisions = (float)ss->numParticles * ss->collisionProb * ss->particleEnergy;

        // Integer part: guaranteed collisions
        int numCollisions = (int)expectedCollisions;
        // Fractional part: probabilistic extra collision
        float fracPart = expectedCollisions - (float)numCollisions;
        if (shakerRand01(&ss->noiseState) < fracPart) {
            numCollisions++;
        }

        // Each collision contributes a random-amplitude impulse
        for (int c = 0; c < numCollisions; c++) {
            float amp = ss->collisionGain * ss->particleEnergy;
            // Random amplitude variation (particles hit at different velocities)
            amp *= (0.7f + 0.6f * shakerRand01(&ss->noiseState));
            impulse += amp * shakerNoise(&ss->noiseState);
        }
    }

    // Feed impulse through resonator bank
    float out = 0.0f;
    for (int i = 0; i < ss->numResonators; i++) {
        out += processShakerResonator(&ss->res[i], impulse, sampleRate);
    }

    return out * ss->soundLevel;
}

// ============================================================================
// BANDED WAVEGUIDE (glass, bowls, bowed bars, chimes — Essl & Cook 1999)
// Algorithm matched to STK BandedWG: tight BiQuad bandpass per mode,
// not KS lowpass. This is what makes glass sound glassy.
// ============================================================================

// Mode frequency ratios per preset (from STK source)
static const float bandedwgModeRatios[BANDEDWG_COUNT][BANDEDWG_NUM_MODES] = {
    // GLASS HARMONICA: cylindrical glass flexural modes (STK preset 3)
    { 1.0f, 2.32f, 4.25f, 6.63f },
    // SINGING BOWL: Tibetan bowl — near-degenerate pairs for beating (STK preset 4, first 4 of 12)
    { 1.0f, 1.002f, 2.98f, 2.99f },
    // VIBRAPHONE: free-free bar modes (STK preset 0 "uniform bar")
    { 1.0f, 2.756f, 5.404f, 8.933f },
    // WINE GLASS: same physics as glass harmonica but played higher
    { 1.0f, 2.32f, 4.25f, 6.63f },
    // PRAYER BOWL: like singing bowl but struck — wider mode spacing
    { 1.0f, 2.71f, 5.13f, 8.27f },
    // TUBULAR: tuned bar modes (STK preset 1 — wide spacing for bell-like tone)
    { 1.0f, 4.0198f, 10.7184f, 18.0697f },
};

// Preset parameter table
typedef struct {
    float baseGains[BANDEDWG_NUM_MODES];   // Per-mode feedback gain (STK basegains)
    float excitation[BANDEDWG_NUM_MODES];  // Per-mode excitation weight
    bool  bowExcitation;    // true=bow, false=strike
    float soundLevel;       // Output scaling
} BandedWGPresetParams;

static const BandedWGPresetParams bandedwgPresetData[BANDEDWG_COUNT] = {
    // GLASS HARMONICA: long sustain, nearly equal gains
    { {0.95f, 0.94f, 0.93f, 0.92f}, {1.0f, 1.0f, 1.0f, 1.0f}, true, 0.40f },
    // SINGING BOWL: paired modes for beating
    { {0.96f, 0.96f, 0.94f, 0.94f}, {1.0f, 1.0f, 1.0f, 1.0f}, true, 0.35f },
    // VIBRAPHONE: aggressive higher-mode decay
    { {0.90f, 0.81f, 0.729f, 0.656f}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0.45f },
    // WINE GLASS: long sustain, pure
    { {0.96f, 0.95f, 0.94f, 0.93f}, {1.0f, 1.0f, 1.0f, 1.0f}, true, 0.50f },
    // PRAYER BOWL: struck, long decay
    { {0.95f, 0.94f, 0.93f, 0.92f}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0.35f },
    // TUBULAR: struck, rapid higher-mode decay for clarity
    { {0.90f, 0.85f, 0.80f, 0.75f}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0.45f },
};

// Initialize banded waveguide preset — partitions ksBuffer, sets up BiQuad bandpass per mode
static void initBandedWGPreset(BandedWGSettings *bw, float *ksBuffer,
                                float freq, float sampleRate, BandedWGPreset preset) {
    if (preset < 0 || preset >= BANDEDWG_COUNT) preset = BANDEDWG_GLASS;

    const BandedWGPresetParams *pp = &bandedwgPresetData[preset];
    const float *ratios = bandedwgModeRatios[preset];

    bw->numModes = BANDEDWG_NUM_MODES;
    bw->bwgBowExcitation = pp->bowExcitation;
    bw->bwgBowVelocity = 0.0f;
    bw->bwgBowPressure = 0.0f;
    bw->strikeEnergy = 0.0f;
    bw->bwgStrikePos = 0.5f;
    bw->soundLevel = pp->soundLevel;
    bw->dcState = 0.0f;
    bw->dcPrev = 0.0f;

    // BiQuad bandpass radius (from STK: 1.0 - PI * 32 / sampleRate)
    float radius = 1.0f - PI * 32.0f / sampleRate;
    if (radius > 0.9999f) radius = 0.9999f;
    if (radius < 0.99f) radius = 0.99f;

    // Partition ksBuffer across modes
    int offset = 0;
    for (int m = 0; m < BANDEDWG_NUM_MODES; m++) {
        bw->modeRatios[m] = ratios[m];
        bw->feedbackGain[m] = pp->baseGains[m];

        // Delay length for this mode
        float modeFreq = freq * ratios[m];
        int len = (int)(sampleRate / modeFreq);
        if (len < 2) len = 2;
        if (offset + len > 2048) len = 2048 - offset;
        if (len < 2) { bw->numModes = m; break; }

        bw->modeOffset[m] = offset;
        bw->modeLen[m] = len;
        bw->modeIdx[m] = 0;

        // Clear delay line segment
        for (int i = 0; i < len; i++) {
            ksBuffer[offset + i] = 0.0f;
        }
        offset += len;

        // BiQuad bandpass resonator coefficients (STK setResonance with normalize=true)
        // All-pole resonator: H(z) = b0 / (1 + a1*z^-1 + a2*z^-2)
        // Peak gain at resonance = b0 / (1 - R^2). Set b0 = (1 - R^2) for unity peak.
        float theta = 2.0f * PI * modeFreq / sampleRate;
        float R = radius;
        bw->bqA1[m] = -2.0f * R * cosf(theta);
        bw->bqA2[m] = R * R;
        // STK normalizes so peak of resonance = 1.0
        // For all-pole 2-pole: peak gain at resonance = 1/(1-R^2)
        // So b0 = (1-R^2) gives unity peak gain
        bw->bqB0[m] = 1.0f - R * R;
        bw->bqY1[m] = 0.0f;
        bw->bqY2[m] = 0.0f;
    }
}

// BiQuad bandpass resonator per mode
// STK BiQuad: out = b[0]*in + b[1]*inZ1 + b[2]*inZ2 - a[1]*outZ1 - a[2]*outZ2
// For bandpass with zeros at z=+1,z=-1: b[0]=b0, b[1]=0, b[2]=-b0
// We store input history in bqY2 (repurposed: bqY1=out[n-1], bqY2=out[n-2])
// and track input[n-2] separately using a simple trick
static inline float bwgBiQuadTick(float input, int m, BandedWGSettings *bw) {
    // Direct form II transposed for the 2-pole 2-zero bandpass
    // Using state: y1=out[n-1], y2=out[n-2]
    // We need input[n-2] but don't store it — use the all-pole form instead
    // which STK actually computes as a simple 2-pole resonator:
    //   out = b0 * input - a1 * y[n-1] - a2 * y[n-2]
    // This is a 2-pole filter (no zeros) which works as a resonator
    float out = bw->bqB0[m] * input - bw->bqA1[m] * bw->bqY1[m] - bw->bqA2[m] * bw->bqY2[m];
    bw->bqY2[m] = bw->bqY1[m];
    bw->bqY1[m] = out;
    return out;
}

// STK bow table: f(x) = pow(abs(x*slope + offset) + 0.75, -4), clamped [0.01, 0.98]
static inline float bwgBowTable(float input) {
    // slope=3.0, offset=0.001 (STK defaults)
    float sample = fabsf(input * 3.0f + 0.001f) + 0.75f;
    sample = 1.0f / (sample * sample * sample * sample);  // pow(-4)
    if (sample > 0.98f) sample = 0.98f;
    if (sample < 0.01f) sample = 0.01f;
    return sample;
}

// Main banded waveguide oscillator (STK-matched algorithm)
static float processBandedWGOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;  // Coefficients computed at init, not per-sample
    BandedWGSettings *bw = &v->bandedwgSettings;
    if (bw->numModes <= 0) return 0.0f;

    // 1. Compute excitation input (same for all modes, per STK)
    float input = 0.0f;

    if (bw->bwgBowExcitation) {
        // Bow: sum all mode delay outputs, compute friction, divide by nModes
        if (v->envStage >= 1 && v->envStage <= 3) {
            float velocitySum = 0.0f;
            for (int m = 0; m < bw->numModes; m++) {
                int off = bw->modeOffset[m];
                int idx = bw->modeIdx[m];
                velocitySum += bw->feedbackGain[m] * v->ksBuffer[off + idx];
            }
            float deltaV = bw->bwgBowVelocity - velocitySum;
            input = deltaV * bwgBowTable(deltaV);
            input /= (float)bw->numModes;
        }
    } else {
        // Strike: decaying noise impulse
        if (bw->strikeEnergy > 0.0001f) {
            input = bw->strikeEnergy * noise() * 0.1f;
        }
    }

    // 2. Process each mode: bandpass(input + gain * delay_out) → delay
    float out = 0.0f;

    for (int m = 0; m < bw->numModes; m++) {
        int off = bw->modeOffset[m];
        int len = bw->modeLen[m];
        int idx = bw->modeIdx[m];
        if (len <= 0) continue;

        // Read delay line output
        float delayOut = v->ksBuffer[off + idx];

        // Bandpass filter: input + feedback * delay_out (STK topology)
        float bpInput = input + bw->feedbackGain[m] * delayOut;
        float bpOut = bwgBiQuadTick(bpInput, m, bw);

        // Safety clamp — prevent runaway (should not trigger with correct gains)
        if (bpOut > 1.0f) bpOut = 1.0f;
        if (bpOut < -1.0f) bpOut = -1.0f;

        // Write bandpass output back into delay line
        v->ksBuffer[off + idx] = bpOut;

        // Advance delay index
        bw->modeIdx[m] = (idx + 1) % len;

        // Accumulate output from bandpass (STK sums bandpass outputs)
        out += bpOut;
    }

    // Decay strike energy
    if (!bw->bwgBowExcitation && bw->strikeEnergy > 0.0f) {
        bw->strikeEnergy *= 0.9995f;
    }

    // Scale output
    out *= 1.0f;

    // DC blocker
    float dcOut = out - bw->dcPrev + 0.995f * bw->dcState;
    bw->dcPrev = out;
    bw->dcState = dcOut;

    return dcOut * bw->soundLevel;
}

#endif // PIXELSYNTH_SYNTH_OSCILLATORS_H
