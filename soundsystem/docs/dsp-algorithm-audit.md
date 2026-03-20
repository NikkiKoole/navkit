# PixelSynth — DSP Algorithm Audit

Complete inventory of every DSP algorithm in the soundsystem: what it is, how it works, where it comes from, and whether it's up to spec.

---

## 1. OSCILLATORS

### 1.1 Standard Waveforms (Square, Saw, Triangle)
**File**: `synth.h:1429–1471`
**Algorithm**: PolyBLEP (Polynomial Band-Limited Step)
**Reference**: Välimäki & Franck, "Polynomial Implementation of Digital Oscillators" (2005); widely used in virtual analog since ~2010.
**Implementation**:
- `polyblep(t, dt)` — 2nd-order polynomial residual applied within one sample of each discontinuity.
- `polyblepSaw(phase, dt)` — naive saw (2p−1) minus one PolyBLEP at wrap.
- `polyblepSquare(phase, pw, dt)` — naive square ±1, PolyBLEP at rising edge (phase=0) and falling edge (phase=pw), with DC correction `-(2pw−1)`.
- `polyblepTriangle(phase, dt, *integrator)` — integrated PolyBLEP'd square with leaky integrator (0.9999 leak = −3 dB at ~0.7 Hz).
**Assessment**: Standard quality. PolyBLEP is the most common alias-free approach for real-time VA. Slightly inferior to PolyBLAMP or minBLEP in HF rejection at high pitches, but perfectly fine for this context. The triangle integration + gentle leak is correct.

### 1.2 Noise
**File**: `synth.h` (`voiceNoise()`)
**Algorithm**: Linear Congruential Generator (LCG)
**Formula**: `state = state * 1103515245 + 12345; output = (state >> 16) / 32768 - 1`
**Reference**: Numerical Recipes LCG (Knuth MMIX variant). The shift-by-16 extracts the better-distributed high bits.
**Assessment**: Adequate for audio noise. Not cryptographically strong (not needed). The spectrum is essentially white. For musical use, this is standard.

### 1.3 Single Cycle Waveform (WAVE_SCW)
**File**: `synth.h:1473+`, `scw_data.h`
**Algorithm**: Wavetable playback with cubic Hermite interpolation
**Interpolation**: 4-point, 3rd-order Hermite spline. Reads 4 surrounding samples, applies Catmull-Rom coefficients.
**Reference**: Standard computer music technique (Roads, "The Computer Music Tutorial", 1996; Smith, "Physical Audio Signal Processing").
**Assessment**: Good. Hermite is a substantial upgrade over linear interpolation — less HF rolloff and no intermodulation artifacts. For single-cycle waves this is ideal. No MIP-mapping / band-limiting of the table itself, so aliasing is possible at very high pitches where harmonics exceed Nyquist — but for typical melodic use the wavetable sizes (up to 2048) give enough oversampling headroom.

### 1.4 Formant Synthesis (WAVE_VOICE)
**File**: `synth_oscillators.h:1–184`
**Algorithm**: Source-filter model. Glottal pulse excitation → 3 parallel bandpass (formant) filters + optional nasal anti-formant.
**Source**: Glottal pulse simulation: `glottal = (t < 0.4) ? sin(t·π/0.4) : −0.3·sin((t−0.4)·π/0.6)`. Blended with smooth triangle via `buzziness` parameter. Breathiness adds noise to source.
**Formant filters**: State-variable filters (Chamberlin SVF) per formant: `fc = 2·sin(π·freq/sr)`, `Q = freq/(bw+1)`. Bandpass output used.
**Formant data**: 5 vowels (A/E/I/O/U) × 3 formants with hardcoded frequency/bandwidth/amplitude. Interpolated via `vowelBlend`.
**Nasal**: Anti-formant notch at 350 Hz (same Chamberlin SVF as notch = LP + HP) + additive sine resonances at 250 Hz and 2500 Hz.
**Reference**: Classic source-filter speech model (Klatt 1980; Fant 1970). The Chamberlin SVF formant filters are from Hal Chamberlin, "Musical Applications of Microprocessors" (1985). Formant frequency tables match standard acoustic phonetics data (Peterson & Barney, 1952).
**Assessment**: Functional and musical. The Chamberlin SVF for the formant filters is slightly dated (not zero-delay feedback) — at high formant frequencies it will have frequency warping, but since formant filters are typically < 3 kHz this is acceptable. The glottal source is a reasonable approximation. For a more physically accurate model you'd want a LF (Liljencrants-Fant) glottal pulse, but the current approach sounds good for synthesis purposes.

### 1.5 Karplus-Strong Plucked String (WAVE_PLUCK)
**File**: `synth_oscillators.h:186–223`, init at `1497–1523`
**Algorithm**: Karplus-Strong with Thiran allpass fractional delay
**Core loop**: Read from delay line → average with next sample (the KS lowpass) → brightness blend (linear interp between averaged and direct) → 1-pole damping LP → Thiran allpass for fractional pitch correction → write back.
**Fractional delay**: First-order Thiran allpass. Coefficient `C = (1−frac)/(1+frac)`. Transfer function `H(z) = (C + z⁻¹) / (1 + C·z⁻¹)`. Implemented in transposed direct form II.
**Excitation**: Noise burst fills delay buffer at note-on.
**Reference**: Karplus & Strong, "Digital Synthesis of Plucked-String and Drum Timbres" (1983). Jaffe & Smith, "Extensions of the Karplus-Strong Plucked-String Algorithm" (1983). Thiran allpass: Laakso et al., "Splitting the Unit Delay" (1996).
**Assessment**: Solid implementation. The Thiran allpass is the correct approach for fractional delay — without it, high-pitched plucks would be audibly detuned (up to 39 cents at 4 kHz, as noted in the code comments). The brightness/damping controls add useful timbral shaping. This is essentially the standard KS implementation found in STK and other reference implementations.

### 1.6 Bowed String (WAVE_BOWED)
**File**: `synth_oscillators.h:225–277`, init at `1525–1554`
**Algorithm**: Smith/McIntyre digital waveguide with nonlinear friction
**Structure**: Two delay lines (nut-side, bridge-side) meet at bow contact point. Bow injects energy via friction function.
**Friction**: `f(Δv) = pressure · Δv · exp(−pressure · Δv²)` — hyperbolic stick-slip model. Linear at small differential velocity (stick), falls off at high velocity (slip).
**Reflections**: Nut = fixed end (invert + 1-pole LP, coeff 0.35/0.65). Bridge = invert × 0.995 + LP (coeff 0.15/0.85). Asymmetric reflections give body character.
**Reference**: Smith, "Physical Audio Signal Processing" (CCRMA online textbook). McIntyre, Schumacher & Woodhouse, "On the Oscillations of Musical Instruments" (1983). Cook, "Real Sound Synthesis for Interactive Applications" (2002). STK `Bowed` class (Scavone).
**Assessment**: Correct waveguide topology. The friction function is a simplified version of the McIntyre model (the full model has a more complex multi-valued friction curve). The reflection filters are reasonable but simple — a more sophisticated model would use higher-order filters to model frequency-dependent loss. However, for real-time game audio this is a good balance of quality vs. cost. No fractional delay on the waveguides (integer sample delay only), which limits pitch accuracy at high frequencies.

### 1.7 Blown Pipe (WAVE_PIPE)
**File**: `synth_oscillators.h:279–333`, init at `1556–1590+`
**Algorithm**: Fletcher/Verge jet-drive model (flute-style bore oscillator)
**Structure**: Single bore delay line + jet delay (3–63 samples) + nonlinear jet deflection via `tanh()`.
**Jet model**: `excitation = tanh(jetOut · gain) · breath`. The tanh provides the S-curve switching behavior across the labium edge.
**Feedback**: Bore return → open-end reflection (invert × 0.9 + LP) → mouth-end coupling (gain = 0.5 + embou × 0.4) → jet delay → nonlinear → bore input.
**DC blocking**: `dcOut = out − dcPrev + 0.995 · dcState` (1st-order HP at ~3.5 Hz).
**Reference**: Fletcher & Rossing, "The Physics of Musical Instruments" (1998). Verge et al., "Jet Formation and Oscillation in a Flue Organ Pipe" (1994). Cook, STK `Flute` class. The jet-drive model is a well-established simplification of the real jet physics.
**Assessment**: Standard STK-derived flute model. The bore is a single delay line (no upper reflections used in this version), which is correct for a simple cylindrical bore. The jet delay and tanh nonlinearity are the canonical approach. Overblow control via gain is correct (higher gain → more energy at higher modes). No tonehole lattice, so this models a simple open pipe. Perfectly adequate for the application.

### 1.8 Additive Synthesis (WAVE_ADDITIVE)
**File**: `synth_oscillators.h:335–516`
**Algorithm**: Bank of sine oscillators with per-harmonic amplitude, frequency ratio, and decay.
**Presets**: Sine, Organ (drawbar), Bell (inharmonic), Strings (1/n rolloff + shimmer), Brass (odd-harmonic emphasis), Choir (warm + shimmer).
**Features**: Brightness scaling via `1/(i+1)^falloff`, inharmonicity via `stretch = 1 + inh·(ratio−1)²`, shimmer via random noise on phase.
**Anti-aliasing**: Harmonics above Nyquist are skipped (`if harmFreq >= sr/2 continue`).
**Reference**: Standard additive synthesis (Risset & Mathews, "Analysis of Musical-Instrument Tones", 1969). The stiffness inharmonicity model is from Fletcher & Rossing for vibrating bars/strings.
**Assessment**: Clean implementation. The hard Nyquist cutoff (skip harmonics above sr/2) avoids aliasing. The brightness power-law scaling and 1/n spectra are standard. Bell inharmonic ratios are reasonable approximations. The organ drawbar mapping is correct (Hammond footage positions). Up to 16 harmonics max, which is sufficient for most timbres but limits very bright sounds at low fundamentals.

### 1.9 Mallet Percussion (WAVE_MALLET)
**File**: `synth_oscillators.h:518–746`
**Algorithm**: Modal synthesis — 4 damped sinusoidal modes with configurable frequency ratios
**Mode ratios** (ideal bar): 1.0, 2.758, 5.406, 8.936 — the first four flexural modes of a uniform bar.
**Presets**: Marimba (tuned 1:4:10:20 undercut), Vibes (1:4:10:20 aluminum), Xylophone (1:3:9:18 rosewood), Glockenspiel (1:2.9:5.8:9.5 steel), Tubular Bells (1:2:3:4 tubes).
**Features**: Per-mode exponential decay, strike position via cosine node patterns, hardness scales higher mode amplitudes, resonance boosts overall level, vibraphone tremolo (motor AM).
**Reference**: Fletcher & Rossing, "The Physics of Musical Instruments" (1998), Chapter 19 (Bars). The undercut tuning ratios (4:1 for mode 2) are industry-standard marimba tuning. Modal synthesis approach from Adrien, "The Missing Link: Modal Synthesis" (1991).
**Assessment**: Effective and correct. The ideal bar ratios are from the classic Euler-Bernoulli beam equation solutions. The preset ratios are well-calibrated to real instruments. 4 modes is the minimum for recognizable mallet sounds — more modes (8–12) would add realism for bells and metallophones. The exponential decay is physically correct (radiation + internal damping). The strike position node calculation using cosine is a reasonable approximation (actual Bessel function zeros for a bar are more complex, but cosine captures the essential behavior).

### 1.10 Granular Synthesis (WAVE_GRANULAR)
**File**: `synth_oscillators.h:748–882`
**Algorithm**: Asynchronous granular synthesis from wavetable source (SCW).
**Grain envelope**: Hanning window `0.5·(1 − cos(2π·phase))`.
**Spawning**: Round-robin grain allocation, spawn rate = `grainDensity` grains/sec. Position and pitch randomization.
**Interpolation**: Cubic Hermite for reading source table.
**Normalization**: `out /= sqrt(expectedOverlap)` where overlap = density × size. The sqrt gives perceptually constant loudness.
**Reference**: Roads, "Microsound" (2001) — the definitive text on granular synthesis. Hanning window is the standard grain envelope (minimal spectral leakage while avoiding clicks). The sqrt normalization is from the Gabor/granular amplitude theory.
**Assessment**: Clean asynchronous granular implementation. Max 16 grains (GRANULAR_MAX_GRAINS), which is sufficient for most textures. The cubic Hermite interpolation is good. The randomization is basic (uniform distribution for position and amplitude, semitone-based for pitch) — more advanced implementations might use Gaussian distributions, but uniform is fine for musical use.

### 1.11 FM Synthesis (WAVE_FM)
**File**: `synth_oscillators.h:884–949`
**Algorithm**: 2- or 3-operator FM with 4 algorithm routings
**Algorithms**:
- STACK: mod2 → mod1 → carrier (DX7 alg 1 topology)
- PARALLEL: (mod1 + mod2) → carrier independently
- BRANCH: mod2 → mod1 + carrier (Y-split)
- PAIR: mod1 → carrier, mod2 as additive sine
**Modulation index**: Stored in radians (β = peak phase deviation), converted to phase-space by dividing by 2π for the sine lookup.
**Feedback**: `fbSample × feedback × π` — one-sample feedback loop on mod1.
**Reference**: Chowning, "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation" (1973). The modulation index convention (radians) follows Chowning's original definition. The feedback operator concept is from the Yamaha DX7 (1983).
**Assessment**: Correct FM implementation. The 4 algorithms cover the most useful 2/3-operator topologies. The modulation index in radians is the proper convention. The one-sample feedback delay is standard (DX7 uses the same). The conversion `mi / 2π` for phase-space is correct. Limited to 3 operators max — DX7 has 6, but 2-3 operators cover a vast range of useful FM timbres. The FM LFO (5th LFO modulating modIndex) is a nice addition for evolving FM textures.

### 1.12 Phase Distortion (WAVE_PD)
**File**: `synth_oscillators.h:951–1072`
**Algorithm**: CZ-style phase warping — remap the phase ramp before feeding it to cosine.
**Wave types** (8):
- SAW: compress first half, stretch second half → cos(distortedPhase · π)
- SQUARE: sharpen transitions at 0.25/0.75 via sharpness = 0.5 − d·0.45
- PULSE: narrow active portion via width = 0.5 − d·0.45
- DOUBLEPULSE: two peaks per cycle (sync-like)
- SAWPULSE: saw + pulse combination
- RESO1/2/3: window × cos(resoFreq·phase) — triangle/trapezoid/sawtooth windows with 1–8× resonance
**Reference**: Casio CZ series (1984). The phase distortion technique was patented by Casio (US4658691). The resonant modes (windowed cosine at higher multiples) are directly from the CZ-101's "resonance" waveforms. See also Bristow-Johnson, "Wavetable Synthesis 101" for PD overview.
**Assessment**: Faithful to the CZ approach. The resonant waveforms (RESO1-3) with the triangle/trapezoid/sawtooth windows are exactly how the CZ series created its characteristic resonant sounds. The distortion parameter smoothly morphs between undistorted cosine and fully distorted waveshapes. No band-limiting on the phase-distorted output, so aliasing is possible at high distortion values — but this is inherent to PD synthesis and matches the original CZ hardware behavior.

### 1.13 Membrane Percussion (WAVE_MEMBRANE)
**File**: `synth_oscillators.h:1074–1247`
**Algorithm**: Modal synthesis with 6 circular membrane modes (Bessel function zeros)
**Mode ratios**: 1.0, 1.594, 2.136, 2.296, 2.653, 2.918 — these are the zeros of J₀ (the zeroth-order Bessel function of the first kind), which give the natural frequencies of an ideal circular membrane.
**Pitch bend**: `bendMult = 1 + pitchBend · exp(−t / pitchBendDecay)` — exponential decay characteristic of struck membranes.
**Presets**: Tabla (large pitch bend, high tension), Conga (warm, long sustain), Bongo (bright, snappy), Djembe (wide range), Tom (deep, punchy).
**Strike position**: Center strike emphasizes fundamental, edge emphasizes higher modes. Scaling: `edgeBoost + centerBoost` model.
**Reference**: Fletcher & Rossing, "The Physics of Musical Instruments" (1998), Chapter 18 (Membranes). Bessel function mode ratios from Morse & Ingard, "Theoretical Acoustics" (1968). The pitch-bend model is well-documented for tabla physics (Raman 1934; Rossing & Sykes 1982).
**Assessment**: Good modal model. The 6 Bessel modes are correct for a circular membrane. The tabla's characteristic pitch bend (caused by the loaded membrane — the "shyahi" paste) is well-modeled by the exponential decay. The mode ratios are the standard theoretical values. Real tabla modes deviate from ideal Bessel zeros due to the loading — the preset could be improved by using the measured ratios from Raman's work (which are closer to harmonic due to the loading). The tom preset uses these ideal ratios which is correct for an unloaded membrane.

### 1.14 Bird Vocalization (WAVE_BIRD)
**File**: `synth_oscillators.h:1249–1495`
**Algorithm**: Parameterized chirp generator with frequency sweep, trill modulation, and AHD envelope
**Frequency**: Logarithmic interpolation from startFreq → endFreq over chirpDuration, with exponential curve shaping.
**Trill**: Sine LFO at trillRate Hz, ±trillDepth semitones.
**AM flutter**: Sine modulation at amRate Hz, amDepth depth.
**Harmonics**: Fundamental + optional 2nd and 3rd harmonics (sine waves).
**Envelope**: Attack-hold-decay per note, with gap timing for multi-note patterns.
**Presets**: Chirp (up-sweep), Trill (rapid finch-like), Warble (canary), Tweet (staccato sparrow), Whistle (pure robin), Cuckoo (two-tone descending).
**Reference**: Not based on a specific physical model. General bird vocalization synthesis approach similar to Lucass & Müller, "Synthesis of Bird Song" (various implementations). The logarithmic frequency interpolation is the correct approach for musical pitch sweeps.
**Assessment**: Effective for game ambience. This is a parametric/procedural approach rather than a physical model (a physical model would simulate the syrinx — the bird's vocal organ — using pressure-driven oscillators). The parametric approach is appropriate for a game engine where controllability and CPU efficiency matter more than acoustic accuracy. The 6 presets cover common garden bird sounds well.

### 1.15 Unison / Multi-Oscillator
**File**: `synth.h:2114–2153`
**Algorithm**: Up to 6 extra oscillators at configurable frequency ratios and levels
**Mix modes**: Weighted average (default) or additive sum (for metallic hihat/cowbell sounds)
**Reference**: Standard detuned unison technique used in virtually all analog and VA synths.
**Assessment**: Straightforward. The detuning is ratio-based (not cents-based), which is fine for the metallic percussion use case. For typical unison-lead sounds, a cents-based spread with even distribution (as described in the unison globals) would be more musical. Both modes are available.

---

## 2. FILTERS

### 2.1 Simper/Cytomic SVF (State Variable Filter)
**File**: `synth.h:2367–2400`
**Algorithm**: Topology-preserving transposed direct form II SVF (zero-delay feedback)
**Equations**:
```
g = tan(θ)     where θ = cutoff² × 0.75   (preset-compatible mapping)
k = 2(1 − res × 0.98)                      (damping: 2 = no reso, ~0.04 = self-osc)
a1 = 1/(1 + g(g+k)),  a2 = g·a1,  a3 = g·a2
v3 = input − lp
v1 = a1·bp + a2·v3        (bandpass)
v2 = lp + a2·bp + a3·v3   (lowpass)
bp = 2·v1 − bp            (state update)
lp = 2·v2 − lp            (state update)
```
**Outputs**: LP = v2, BP = v1, HP = input − k·v1 − v2
**State clamping**: `tanh(state) × 4` when |state| > 4 — bounds self-oscillation.
**Reference**: Andrew Simper (Cytomic), "How to write an SVF using the trapezoidal integrator method" (2013, rev. 2018). This is the gold standard for digital VA filters. Also described in Zavalishin, "The Art of VA Filter Design" (2012).
**Assessment**: Excellent. This is the best general-purpose digital filter topology for VA synthesis. Unconditionally stable, correct frequency response at all settings, zero-delay feedback means no tuning error. The cutoff mapping (c²×0.75 → tan) preserves backward compatibility with older presets while giving correct frequency warping. The tanh clamping is a sensible soft-limit that keeps self-oscillation musical rather than explosive. This is industry-standard quality.

### 2.2 4-Pole TPT Ladder Filter
**File**: `synth.h:728–865`
**Algorithm**: 4-stage trapezoidal integrator cascade with global resonance feedback, 2× oversampled
**Topology**: Based on IR3109 OTA (operational transconductance amplifier) circuit — the filter chip in the Roland Juno-6/60/106.
**Core equations** (per sample at 2× rate):
```
g = tan(min(frq, 0.85) × π/2)
g1 = g/(1+g)
G = g1⁴
S = s[0]·g1³ + s[1]·g1² + s[2]·g1 + s[3]
u = (input·comp − k·OTAsat(S)) / (1 + k·G)
```
Then 4 cascaded TPT integrators: `v = g1·(in − s); s += 2v; out = s + v`
**Resonance**: `k = 1.024 × (exp(2.128·res) − 1)` — Juno-6 calibrated exponential curve.
**Frequency compensation**: `comp = (1.96 + 1.06k) / (1 + 2.16k)` — keeps −3 dB point on target as resonance increases.
**OTA saturation**: Padé [1,1] tanh approximant: `x(27+x²) / (27+9x²)`. Matches tanh within ~0.5% for |x| < 2, vastly cheaper than `tanhf()`.
**Oversampling**: 2× via polyphase IIR half-band filters (12 allpass coefficients).
**Adaptive thermal noise**: Seeds self-oscillation when input energy is near zero. Noise amplitude scales inversely with signal energy: `1e-3 / (1 + energy·1000)`.
**HF resonance limiter**: Clamps feedback gain k at high frequencies to model OTA bandwidth rolloff, preventing instability.
**Q compensation**: `comp = 1 + k·0.06` and `g1L = g1·(1 + k·0.0003)` — BA662 VCA drive boost at high resonance.
**True multimode output** via intermediate stage taps (Zavalishin binomial mixing):
- **LP** = lp4 (4-pole, −24 dB/oct)
- **BP** = lp2 (2-pole bandpass, resonant peak tracks cutoff)
- **HP** = u − 4·lp1 + 6·lp2 − 4·lp3 + lp4 (4th-order difference, −24 dB/oct)
The stage taps (lp1–lp4) and pre-filter input (u) are returned via `LadderOutput` struct from `ladderProcessSample`, and the mode selection happens before downsampling so all taps are at 2× rate.
**Reference**: Ported from KR-106 (GPLv3) — a virtual Juno-106 synth. Polyphase IIR coefficients from Laurent de Soras, HIIR library (WTFPL license). TPT ladder theory from Zavalishin, "The Art of VA Filter Design" (2012), Chapter 6. OTA modeling from Stinchcombe, "Analysis of the Moog Transistor Ladder and Derivative Filters" (2008). Multimode tap mixing coefficients (1, −4, 6, −4, 1) are the 4th-order binomial coefficients, also from Zavalishin Ch. 6.
**Assessment**: High quality. The 2× oversampling is essential for a ladder filter (without it, the nonlinear feedback creates audible aliasing at high resonance). The Juno-6 calibrated resonance curve and BA662 Q compensation are nice analog-accurate touches. The Padé tanh approximant is efficient and accurate. The HF resonance limiter prevents the instability that plagues naive ladder implementations. True multimode via stage taps gives proper −24 dB/oct slopes and correct resonant peak tracking for all three modes (LP/BP/HP).

### 2.3 One-Pole Filter (6 dB/oct)
**File**: `synth.h:2352–2356`
**Algorithm**: `state += cutoff × (input − state)`
**Output**: LP = state, HP = input − state
**Reference**: Simplest possible IIR lowpass/highpass. Described in every DSP textbook (e.g., Smith, "Introduction to Digital Filters", 2007).
**Assessment**: Correct and intentional — used for gentle filtering that matches vintage drum machine topology (filter types 3-4). The cutoff parameter maps directly to the pole position. At high cutoff values, this becomes essentially transparent; at low values, it's a gentle slope. No resonance control (by design — 1-pole filters can't resonate).

### 2.4 Resonant Bandpass (Bridged-T / Ringing Filter)
**File**: `synth.h:2357–2366`
**Algorithm**: 2-pole bandpass with feedback, self-oscillating at high resonance
**Equations**:
```
f = 2·sin(cutoff²×0.75),  clamped to 0.99
fb = res×0.99 + 0.01
bp += f × (input − bp − fb×(bp − lp))
lp += f × (bp − lp)
output = bp × (1 + res×2)
```
**Reference**: This is a variant of the Chamberlin SVF with modified feedback path. The `f = 2·sin(θ)` coefficient is the Chamberlin approximation (accurate at low frequencies, unstable above ~sr/6). The bridged-T topology name refers to the analog RC circuit this approximates.
**Assessment**: Functional but slightly dated. The `2·sin(θ)` coefficient has the known Chamberlin frequency warping problem — the filter goes unstable at high cutoff frequencies (hence the 0.99 clamp). For this specific use case (ringing metallic percussion, acid bass resonance) the restricted frequency range is acceptable and the self-oscillation character is desirable. The gain boost `(1 + res·2)` compensates for the resonance narrowing the passband.

### 2.5 Formant Bandpass Filters
**File**: `synth_oscillators.h:40–50`
**Algorithm**: Chamberlin SVF in bandpass mode
**Equations**: `fc = 2·sin(π·freq/sr)`, `Q = freq/(bw+1)`, state variable update.
**Reference**: Hal Chamberlin, "Musical Applications of Microprocessors" (1985).
**Assessment**: Same caveats as 2.4. For formant filtering where center frequencies are typically below 3 kHz, the Chamberlin approximation is adequate. The Q calculation is correct (center frequency divided by bandwidth).

### 2.6 Bus SVF Filter
**File**: `effects.h` (bus processing section)
**Algorithm**: Same Simper/Cytomic SVF as voice filter (section 2.1), with identical equations.
**Modes**: LP, HP, BP, Notch (notch = LP + HP = input − k·BP)
**Assessment**: Same excellent quality as the voice filter.

---

## 3. ENVELOPE & MODULATION

### 3.1 ADSR Envelope
**File**: `synth.h` (processVoice, env stages 1–4)
**Algorithm**: 4-stage ADSR with linear attack, linear or exponential decay/release.
**Attack**: Linear ramp, `envLevel = envPhase / attack`.
**Decay**: Linear: `sustain + (1−sustain)·(1 − phase/decay)`. Exponential: `sustain + (1−sustain)·exp(−phase / (decay×0.368))`. The 0.368 factor gives a time constant where the envelope reaches ~1/e at the specified decay time.
**Release**: Linear: ramp from releaseLevel to 0. Exponential: `releaseLevel·exp(−k·phase)` where `k = ln(releaseLevel/0.001) / release`.
**One-shot mode**: Skips sustain, goes directly to release.
**Reference**: Standard ADSR design. The exponential option with the 0.368 scaling matches the analog RC discharge time constant convention.
**Assessment**: Good. The dual linear/exponential mode covers both punchy drums (linear) and musical instruments (exponential). The one-shot mode is correct for percussion. The exponential release calculation properly handles variable release start levels.

### 3.2 Filter Envelope (AD)
**File**: `synth.h:2246–2273`
**Algorithm**: Attack-decay only (no sustain). Linear attack → linear decay to zero.
**Modulation**: `filterEnvAmt × envLevel` added to cutoff.
**Assessment**: Standard filter envelope. The lack of sustain is intentional — filter envelopes in most subtractive synths are AD or AHD.

### 3.3 LFO (5 shapes, free or tempo-synced)
**File**: `synth.h` (processLfo)
**Shapes**: Sine (`sin(2π·phase)`), Triangle (`4·|phase−0.5|−1`), Square (`phase < 0.5 ? 1 : −1`), Sawtooth (`1 − 2·phase`), Sample & Hold (random value held between phase wraps).
**Sync**: Free Hz or tempo-synced divisions (1/32 to 32 bars).
**Per-LFO phase offset**: 0.0–1.0, allows inverted or offset LFO relative to others.
**Reference**: Standard LFO design used in virtually all synthesizers.
**Assessment**: Complete set of standard shapes. The tempo sync with slow divisions (8/16/32 bar) is a nice touch for ambient/evolving textures. No smoothing on S&H transitions (could add a slew limiter for smoother random modulation, but the raw S&H is the classic behavior).

### 3.4 Pitch Envelope
**File**: `synth.h` (voice processing), `synth_oscillators.h:89–113`
**Algorithm**: Amount (semitones) → time → curve. Envelope goes from `pitchEnvAmount` semitones toward 0 over `pitchEnvDecay` time.
**Curve types**: Linear (curve=0), exponential out (`pow(1−t, power)`, curve < 0), exponential in (`pow(t, power)`, curve > 0).
**Two modes**: Semitone (musical pitch via `pow(2, semitones/12)`) and linear Hz (for drums: `base + ratio·exp(−t/(decay·0.368))`).
**Reference**: Standard pitch envelope as found in analog synths and drum machines.
**Assessment**: The dual curve modes (exponential in/out plus linear) give good control. The drum mode with exponential Hz decay is correct for kick drum pitch drops. The semitone mode with 2^(st/12) is the correct equal-temperament conversion.

### 3.5 Glide/Portamento
**File**: `synth.h` (voice frequency update)
**Algorithm**: Exponential approach in log-frequency space.
**Rate**: `speed = glideRate × dt × 6.0`, applied as multiplicative factor toward target frequency.
**Snap detection**: When frequency ratio is within 0.9999–1.0001, snap to target and clear glide.
**Reference**: Standard exponential portamento (constant-time glide, as opposed to constant-rate).
**Assessment**: Correct. Exponential glide in log-frequency space gives musically correct constant-time portamento regardless of interval size. The snap detection prevents indefinite asymptotic approach.

### 3.6 303 Acid Mode
**File**: `synth.h:2299–2310`
**Algorithm**: Accent sweep (RC discharge at ~147 ms time constant) + gimmick dip (negative cutoff offset after filter decay completes).
**Accent sweep**: `level *= exp(−(1/0.147)·dt)` — models the capacitor discharge in the TB-303's accent circuit.
**Gimmick dip**: `−gimmickDipAmt × 0.3` when filter envelope reaches zero — models the VCA bias shift that creates the characteristic "squelch" at the end of each note.
**Reference**: The 147 ms RC time constant is from Robin Whittle's TB-303 circuit analysis. The gimmick dip is documented in the Devil Fish mod (also Whittle). See also "Deconstructing the TB-303" (Sound On Sound).
**Assessment**: Accurate to the real circuit behavior. The 147 ms time constant and gimmick dip are the two most important accidental features of the 303 that define its sound. These are correctly implemented as post-filter modulations on the cutoff.

### 3.7 Arpeggiator
**File**: `synth.h` (arp processing)
**Modes**: Up, Down, UpDown (alternating), Random.
**Rate**: Beat-synced (1/4, 1/8, 1/16, 1/32) or free Hz.
**Chord types**: Octave, Major, Minor, Dom7, Min7, Sus4, Power.
**Beat tracking**: Accumulating beat position (drift-free).
**Assessment**: Standard arpeggiator. The drift-free beat accumulation is correct (avoids the timing drift that occurs with timer-reset approaches).

---

## 4. NONLINEAR PROCESSING (Voice Level)

### 4.1 Distortion / Waveshaping
**File**: `synth.h:36–67`
**Modes** (6):
| Mode | Algorithm | Character |
|------|-----------|-----------|
| Soft | `tanh(x·drive)` | Warm tube-like |
| Hard | Clamp to ±1 | Digital, harsh |
| Foldback | Triangle-fold at ±1 | Metallic, buzzy |
| Rectify | `|tanh(x·drive)|` | Octave-up, industrial |
| Asymmetric | `tanh(x)` pos / `tanh(x·0.6)·0.8` neg | Even harmonics |
| BitFold | Quantize to 32 levels + fold | Lo-fi industrial |
**Reference**: Standard waveshaping techniques. Tanh soft-clipping is the canonical tube saturation approximation (Pakarinen & Yeh, "A Review of Digital Techniques for Modeling Vacuum-Tube Guitar Amplifiers", 2009).
**Assessment**: Good variety. The asymmetric mode correctly generates even harmonics (the asymmetry creates 2nd, 4th, etc. harmonics). The foldback loop (`while`) should have an iteration limit to prevent infinite loops with extreme drive values (currently unbounded, though in practice it converges).

### 4.2 Wavefolding
**File**: `synth.h:2218–2227`
**Algorithm**: Triangle-wave fold. `s = s − floor(s·0.25+0.25)·4; if s>3: s−=4; elif s>1: s=2−s`
**Reference**: West Coast synthesis technique (Buchla/Serge folder circuits). The mathematical triangle fold is a simplified version of the real circuit behavior.
**Assessment**: Correct triangle fold mapping — any input value maps back to [−1, 1]. The gain scaling `1 + wavefoldAmount·4` is appropriate. Like the original analog circuits, this produces rich harmonic content. No oversampling on the folder, so aliasing is possible at high fold amounts — this is the main quality limitation.

### 4.3 Ring Modulation
**File**: `synth.h:2212–2216`
**Algorithm**: `sample × sin(ringModFreq × baseFrequency / sr × 2π)`
**Reference**: Standard ring modulation (multiplication of two signals). Produces sum and difference frequencies.
**Assessment**: Straightforward and correct.

### 4.4 Tube Saturation
**File**: `synth.h:2229–2236`
**Algorithm**: Asymmetric soft clipping + explicit 2nd harmonic.
**Positive**: `1 − exp(−x)`. **Negative**: `−1 + exp(x)`. Plus `0.1·sin(2π·x)`.
**Reference**: The `1−exp(−x)` curve is a standard tube saturation approximation. The explicit 2nd harmonic sine term directly injects even-order distortion.
**Assessment**: The `1−exp(-x)` / `−1+exp(x)` curves are not symmetrically "tube-like" — they create a hard asymmetry that generates strong even harmonics, which is correct for single-ended tube stages. The added sine term is a bit unusual (most implementations rely on the asymmetry alone), but it reinforces the 2nd harmonic explicitly. Effective for warm coloring.

### 4.5 Analog Variance (Per-Voice Component Tolerances)
**File**: `synth.h:2806–2848` (initVoiceCommon)
**Algorithm**: Deterministic per-voice random offsets seeded by voice index, applied at note-on. Models capacitor/resistor tolerances in analog polyphonic synths.
**Hash function**: `seed = voiceIdx × 2654435761` (Knuth multiplicative hash), then chained via `seed = seed × 2246822519 + 374761393` per parameter. Output mapped to −1..+1 via `(seed & 0xFFFF) / 65535 × 2 − 1`.
**Parameters affected**:
| Parameter | Variance | Perceptual effect |
|-----------|----------|-------------------|
| Pitch | ±3 cents | Adds width to chords, slight natural detuning |
| Filter cutoff | ±5% | Each voice has slightly different brightness |
| Envelope timing (A/D/R) | ±8% each | Subtle timing differences, less mechanical |
| Volume | ±0.5 dB (±6%) | Natural level variation between voices |
**Key design**: Deterministic — same voice index always gets the same offsets. This means the variance is consistent across note-ons (voice 0 is always slightly sharp, voice 3 always slightly dark, etc.), which matches real analog hardware where each voice card has fixed component values.
**Reference**: Models the behavior of real polyphonic analog synths (Prophet-5, Juno-106, OB-X) where each voice has slightly different component values. The ±3 cent pitch variance is based on measurements of real Prophet-5 voices (typically ±2–5 cents). Curtis CEM3340 VCO spec allows ±2% frequency error. The ±5% cutoff variance matches CEM3320 VCF component tolerances.
**Assessment**: Well-calibrated. The variance amounts are subtle enough to add life without sounding "broken". The deterministic seeding is the correct approach (random-per-note would sound different, more like a random modulation than fixed hardware character). The multiplicative hash gives good distribution across voice indices. **Possible enhancement**: Component drift over time (temperature-dependent tuning shift) — real analog synths drift as they warm up, which could be modeled with a slow global offset that changes over the first few minutes.

### 4.6 Analog Rolloff
**File**: `synth.h:2238–2243`
**Algorithm**: 1-pole lowpass with fixed coefficient 0.15 (~12 kHz at 44.1 kHz).
**Formula**: `state += 0.15 × (input − state)`
**Reference**: Models the natural HF rolloff of analog circuits (op-amp bandwidth, wire capacitance).
**Assessment**: Simple and effective. The −3 dB point at ~12 kHz matches the typical bandwidth of vintage analog synths. This removes the "digital harshness" above 12 kHz that can make VA oscillators sound sterile.

---

## 5. MASTER EFFECTS CHAIN

Signal flow: **Distortion → Bitcrusher → Chorus → Flanger → Phaser → Comb → Tape → Vinyl → Delay → [Dub Loop] → Rewind → TapeStop → BeatRepeat → DJFX Loop → Reverb → HalfSpeed**

### 5.1 Distortion
**File**: `effects.h` (processDistortion)
**Algorithm**: Same waveshaping modes as voice distortion (section 4.1) + post-distortion 1-pole LP tone control.
**Tone**: `cutoff = tone²·0.5 + 0.1` — exponential curve, darker at low values.
**Assessment**: Adequate. The tone control after distortion is essential for taming harshness. Same assessment as 4.1.

### 5.2 Bitcrusher
**File**: `effects.h` (processBitcrusher)
**Algorithm**: Sample rate reduction + bit depth reduction.
**Rate reduction**: Hold sample value for `crushRate` consecutive samples (zero-order hold).
**Bit reduction**: `floor(sample × levels) / levels` where `levels = 2^bits`.
**Reference**: Standard bitcrusher algorithm. The zero-order hold is the simplest approach (more sophisticated approaches would use decimation + interpolation).
**Assessment**: Correct. The zero-order hold creates the characteristic staircase aliasing. The bit reduction correctly quantizes to N-bit resolution. No dithering is applied (which is intentional — the quantization noise IS the effect).

### 5.3 Chorus (Classic + BBD Mode)
**File**: `effects.h:1218–1310` (processChorus + bbdSaturate)
**Algorithm**: Two selectable modes — Classic (dual sine LFO) or BBD (Juno-style bucket brigade delay emulation).

**Classic mode**:
- Dual sine LFOs at 1.0× and 1.1× rate (creates spatial width via detuning).
- Hermite interpolation on delay buffer.
- Optional feedback for flanging-style effects.

**BBD mode** (`chorusBBD = true`):
- Single triangle LFO with cubic soft-clip at peaks: `tri = tri × (1.5 − 0.5·tri²)`. Models the capacitor rounding in the Juno's clock driver — real BBD clock waveforms are never perfectly linear.
- Antiphase taps: tap1 = +LFO, tap2 = −LFO (180° apart). This is the exact topology of the Juno-6 chorus: one BBD line, two antiphase outputs mixed for stereo.
- Charge-well saturation (`bbdSaturate`): soft clips above ±0.7 via `threshold + excess/(1 + excess·3)`. Models the cumulative nonlinearity across BBD stages where each charge well has a limited voltage swing, adding warm even harmonics.
- Saturation also applied in feedback path (BBD feedback goes through the same charge wells).
- Hermite interpolation on bus chorus, linear interp on classic bus chorus.

**Constants**: `BBD_CHARGE_SAT = 0.7` (charge-well threshold), `BBD_ANTIPHASE = 0.5` (180° offset).
**Delay range**: 5–30 ms. Buffer: 2048 samples (~46 ms at 44.1 kHz).
**Reference**: Roland Juno-6/60/106 chorus circuit (MN3009 BBD chip). The antiphase stereo topology is documented in the Juno-6 service manual. BBD charge saturation modeling from Raffel & Smith, "A Simple Bucket-Brigade Device Model" (DAFx 2010). The cubic-clipped triangle LFO models the integrator capacitor in the Juno's clock circuit (Stinchcombe, "Analysis of BBD-Based Audio Effects", 2016).
**Assessment**: Good Juno-style chorus. The antiphase single-LFO topology is historically accurate (the Juno uses exactly this). The charge-well saturation is the key to the BBD warmth — without it, a BBD chorus just sounds like a regular digital chorus. The cubic soft-clip on the triangle LFO is a nice touch that captures the rounded peaks of real BBD clock waveforms. **Note**: A more complete BBD model would include clock feedthrough noise (the characteristic "ticking" at low rates) and frequency-dependent signal loss across stages, but these are subtle effects that may not be worth the CPU cost. Both master and per-bus chorus support BBD mode.

### 5.4 Flanger
**File**: `effects.h` (processFlanger)
**Algorithm**: Short modulated delay with feedback, triangle LFO.
**Delay range**: 0.1–10 ms. Buffer: 512 samples (~11 ms).
**LFO**: Triangle wave (smoother than sine for flanging sweeps).
**Feedback**: ±0.95 (negative inverts phase → hollow "through-zero" character).
**Soft clipping**: Feedback clamped to ±1.5.
**Reference**: Standard flanger design. The through-zero flanging with negative feedback is the classic technique.
**Assessment**: Correct. The triangle LFO is the standard choice for flangers (smoother sweep than sine). Feedback clamping prevents runaway. Short delay times and high feedback create the characteristic jet-engine sound.

### 5.5 Phaser
**File**: `effects.h` (processPhaser)
**Algorithm**: Cascade of 2–8 first-order allpass filters, LFO modulates allpass coefficient.
**Allpass**: `output = −coeff × input + prev; prev = coeff × output + input` (standard first-order allpass).
**LFO**: Sine wave modulates coefficient from 0.1 to 0.9.
**Feedback**: Output → input (0 to 0.9).
**Reference**: Standard phaser design. See Smith, "Physical Audio Signal Processing", chapter on allpass filters.
**Assessment**: Correct. Each allpass stage creates one notch in the frequency response. 4 stages (classic) = 2 notches, 8 stages = 4 notches. The sine LFO sweeps the notch frequencies for the characteristic sweeping effect. The allpass implementation is the standard textbook form.

### 5.6 Comb Filter
**File**: `effects.h` (processComb)
**Algorithm**: Feedback delay line tuned to a specific pitch, with damping LP in feedback path.
**Delay**: `samples = sr / frequency` (20–2000 Hz range).
**Feedback**: −0.95 to +0.95 (negative = hollow/nasal).
**Damping**: 1-pole LP on feedback (frequency-dependent loss per iteration).
**Reference**: Standard comb filter. This is essentially the same topology used in Karplus-Strong synthesis and in the reverb's comb filters.
**Assessment**: Correct. The damping LP in the feedback path prevents infinite ringing at high frequencies, which makes the resonance more natural-sounding. Negative feedback creates the "hollow" character by emphasizing odd harmonics.

### 5.7 Tape Simulation
**File**: `effects.h` (processTape)
**Algorithm**: Saturation + wow/flutter + hiss.
**Saturation**: `tanh()` soft clipping.
**Wow**: Sine LFO at 0.5 Hz, ±200 samples (±4.5 ms) pitch modulation via modulated delay read.
**Flutter**: Sine LFO at 6 Hz, ±40 samples (±0.9 ms).
**Hiss**: LP-filtered white noise (3 kHz corner).
**Buffer**: 4096 samples (~93 ms) circular delay.
**Reference**: Standard tape simulation approach. Wow/flutter rates and depths match typical cassette player measurements. See Välimäki & Bilbao, "Virtual Analog Effects" (2010).
**Assessment**: Functional. The wow/flutter are sine-based (real tape machines have more irregular speed variations — a noise-modulated LFO would be more realistic). The saturation is basic tanh (real tape has frequency-dependent saturation — more compression at low frequencies). For a game audio context, this is adequate. The hiss spectrum is correct (tape hiss is predominantly mid-high frequency).

### 5.8 Vinyl Simulation
**File**: `effects.h` (processVinylSim)
**Algorithm**: Pitch warp + surface noise + crackle + 2-pole tone LP.
**Warp**: Slow sine LFO modulates delay read position (±150 samples = ±3.4 ms).
**Noise**: LP-filtered white noise (3 kHz corner).
**Crackle**: Sparse pops (random noise > threshold, sparse gating).
**Tone**: Cascaded 2× 1-pole LP (800 Hz–16 kHz range).
**Reference**: Common vinyl simulation technique. See Esquef & Välimäki, "Interpolation of Long Gaps in Audio Signals" for vinyl noise modeling.
**Assessment**: Decent for character. The crackle model is simplistic (real vinyl crackle has a specific transient shape and spectral content related to physical groove damage). The 2-pole tone filter correctly models the HF rolloff of vinyl playback (RIAA-ish). The pitch warp captures the basic "warped record" effect.

### 5.9 Delay
**File**: `effects.h` (processDelay)
**Algorithm**: Circular buffer with Hermite interpolation, filtered feedback.
**Buffer**: 2× sr samples (~2 seconds at 44.1 kHz).
**Interpolation**: Cubic Hermite for smooth delay time modulation.
**Feedback filter**: 1-pole LP with exponential tone control (`tone²·0.4 + 0.1`).
**Reference**: Standard digital delay design.
**Assessment**: Good. Hermite interpolation prevents clicks when delay time is modulated. The tone filter on feedback creates progressively darker repeats, which is the standard behavior of tape and analog delays.

### 5.10 Send Delay
**File**: `effects.h` (_processDelaySendCore)
**Algorithm**: Identical to master delay but in a separate buffer, always running.
**Use**: Shared delay effect fed by per-bus delaySend knobs (send/return architecture).
**Assessment**: Correct send/return topology.

---

## 6. REVERB

Two selectable algorithms via `reverbFDN` toggle (A/B testable in the DAW UI). Both share the same parameter interface (size, damping, mix, pre-delay, bass).

### 6.1 Schroeder Reverberator (reverbFDN = false, default)
**File**: `effects.h:1481–1491` (`_processReverbCore`)
**Algorithm**: 4 parallel comb filters → 2 series allpass filters, with pre-delay.
**Comb lengths**: 1559, 1621, 1493, 1427 samples (mutually prime to avoid flutter).
**Allpass lengths**: 223, 557 samples.
**Pre-delay**: 0–100 ms (4410 samples max).
**Comb processing**: `output = buffer[pos]; lpState = output·(1−damp) + lpState·damp; buffer[pos] = input + lpState·feedback; pos = (pos+1)%size`
**Allpass processing**: `output = −coeff·input + buffer[pos]; buffer[pos] = input + coeff·output; pos = (pos+1)%size`
**Feedback**: `REVERB_FEEDBACK_MIN + REVERB_FEEDBACK_RANGE × roomSize` = 0.7–0.95.
**Bass control**: Separate feedback multiplier for comb filters (reverbBass × feedback), giving richer low-frequency sustain.
**Reference**: Schroeder, "Natural Sounding Artificial Reverberation" (1962). Moorer, "About This Reverberation Business" (1979) — added damped feedback in combs. The mutually-prime comb lengths prevent flutter echo (Jot & Chaigne, 1991).
**Assessment**: Classic Schroeder/Moorer reverb — well-understood, CPU-efficient, reasonable results. The damping LP in the comb feedback is the Moorer improvement for frequency-dependent decay. **Limitations**: Known for metallic coloration at short decay times, low echo density (only 4 independent combs), and no early reflections. Comb lengths are tuned for 44100 Hz only.

### 6.2 FDN Reverberator (reverbFDN = true)
**File**: `effects.h` (`_processReverbFDN`, `fdnHadamard8`)
**Algorithm**: 8-line Feedback Delay Network with Hadamard mixing matrix + early reflections tap line.

**Architecture**:
```
input → pre-delay → early reflections (6-tap delay) ──────┐
                  → 8 delay lines → Hadamard matrix ──┐   │
                       ↑          ← damping LP ← fb ←─┘   │
                       └── DC blocker ← input injection    │
                                                           │
output = early × 0.5 + tail (sum of 8 lines × 1/8) ←─────┘
```

**Delay lines**: 8 lines with mutually-prime lengths: 1327, 1631, 1973, 2311, 2677, 3001, 3359, 3671 samples (30–83 ms at 44.1 kHz). Spread across a wide range to avoid modal clustering.

**Hadamard matrix**: 8×8 orthogonal mixing matrix applied in-place via 3-stage butterfly (Walsh-Hadamard transform). Each stage is pairs of `(a+b, a−b)` operations — no multiplications, only additions/subtractions. Normalized by `1/√8 ≈ 0.3536`. The Hadamard matrix is:
- **Orthogonal**: energy-preserving (no buildup or decay from the matrix itself)
- **Maximally coupled**: every line feeds into every other line, creating exponential echo density growth
- **Lossless**: the matrix has eigenvalues of ±1 only

**Early reflections**: 6-tap delay line with fixed tap times (210–1567 samples) and inverse-distance gains (0.50–0.17). Tap times scaled by room size (0.3–1.0× original). Models first-order reflections from walls/ceiling of a medium room (~5×7×3 m, image-source approximation).

**Per-line processing**:
- **Damping LP**: Same 1-pole topology as Schroeder combs (`state = x·dampCoef + state·(1−dampCoef)`), same damping parameter for compatibility.
- **Feedback**: Same `REVERB_FEEDBACK_MIN + size × REVERB_FEEDBACK_RANGE` formula. First 4 lines (longer delays, lower-frequency modes) use `bassFeedback` (= feedback × reverbBass); last 4 use normal feedback.
- **DC blocker**: Leaky integrator tracks DC component (`dcState = dcState·0.995 + x·0.005`), subtracted from signal. Prevents low-frequency buildup in the feedback loop.
- **Input injection**: Pre-delayed input distributed equally to all 8 lines (× 0.35 gain per line).

**Output**: `early × 0.5 + tail`, where tail = sum of all 8 line outputs × 1/8.

**Reference**:
- Jot, "Efficient Structures for Reverb Processing" (1992) — original FDN formulation with unitary mixing matrices.
- Smith, "Physical Audio Signal Processing" (CCRMA online textbook) — FDN chapter.
- Schlecht & Habets, "On Lossless Feedback Delay Networks" (2017) — theory of orthogonal FDN matrices.
- Hadamard/Walsh-Hadamard transform: standard signal processing (Beauchamp & Walsh, 1975). The butterfly decomposition is equivalent to the Cooley-Tukey FFT structure applied to ±1 matrices.

**Assessment**: Major quality improvement over Schroeder. The 8 cross-coupled lines produce dramatically higher echo density — echoes multiply exponentially through the matrix rather than recirculating independently. The early reflections add the spatial cue that Schroeder completely lacks (the ear uses early reflections to judge room size and shape). The Hadamard matrix is the optimal choice for FDN: orthogonal (energy-preserving), maximally diffusing (all lines couple to all others), and computationally free (no multiplications — just ± additions via the butterfly). Same CPU cost as the Schroeder (8 delay reads/writes + 24 additions for the butterfly vs. 4 comb + 2 allpass reads/writes), but qualitatively superior on every metric: density, smoothness, spatial impression, and absence of metallic coloration. **Same limitations as Schroeder**: delay lengths are fixed for 44100 Hz. At other sample rates, lengths should be scaled proportionally.

---

## 7. DUB LOOP (King Tubby Tape Delay)

**File**: `dub_loop.h` (~310 lines)
**Algorithm**: Multi-head tape delay with cumulative degradation, wow/flutter, and per-head drift.

### 7.1 Tape Buffer
**Size**: 4 seconds × sr. Fractional write/read positions.
**Write speed**: Controlled by `speed` parameter (0.5–2.0×), smoothed via slew toward target.
**Read heads**: Up to 3, each at independent fractional positions with Hermite interpolation.
**Head timing**: Slews toward target with ~50 ms inertia (prevents clicks on delay time changes).

### 7.2 Degradation Model
**Echo age**: Accumulated counter (0–8, 0.5/sec × degradeRate).
**Per-echo**: Saturation (tanh at age-dependent gain), tone loss (LP darkens per pass), noise addition (hiss increases per echo).
**Reference**: Models the physical degradation of tape echo machines (Space Echo, Copicat). Each pass through the tape head adds saturation, noise, and HF loss. This cumulative approach is correct.
**Assessment**: Good character model. The age cap at 8 prevents unbounded degradation. The saturation → tone loss → noise pipeline is the correct order for tape echo aging.

### 7.3 Wow/Flutter
**Wow**: 0.4 Hz sine, ±40 samples. **Flutter**: 5 Hz sine, ±8 samples. Combined offset applied to read position.
**Assessment**: Standard tape speed variation model. Same note as tape simulation — sine LFOs are simpler than real tape mechanics but adequate.

### 7.4 Delay Time Drift
**Per-head**: Independent dual-sine LFOs at `driftRate × (0.8–1.2)` Hz per head. `drift1·0.7 + drift2·sin(2.3·phase)·0.3` for organic feel.
**Assessment**: The dual-sine with irrational ratio (2.3×) creates a quasi-random drift pattern that avoids obvious periodicity. Good approach for the "woozy" character.

---

## 8. REWIND / TAPE STOP / PERFORMANCE EFFECTS

### 8.1 Rewind (Vinyl Spinback)
**File**: `rewind.h` (~165 lines)
**Algorithm**: Continuous circular capture → triggered reverse playback with speed curve.
**Speed curves**: Linear (`1 − progress·(1−minSpeed)`), Exponential (`minSpeed + (1−minSpeed)·exp(−progress·4)`), S-curve (quadratic blend).
**Character**: Speed wobble (noise-modulated), LP sweep (darkens as speed drops), vinyl crackle (sparse pops).
**Crossfade**: 50 ms smooth transitions in/out.
**Assessment**: Well-designed DJ effect. The exponential curve gives the most natural brake feel. The filter sweep tied to speed is correct (real turntable spinback has this physical property — slower platter = less HF). The crossfade prevents clicks.

### 8.2 Tape Stop
**File**: `effects.h` (tape stop state machine)
**Algorithm**: Same capture/playback as rewind but with deceleration → stop → optional spinback.
**States**: Idle → Stopping → Stopped → Spinback.
**Speed curves**: Same 3 curves as rewind.
**Assessment**: Correct implementation of the classic tape stop effect.

### 8.3 Beat Repeat / Stutter
**File**: `effects.h` (beat repeat)
**Algorithm**: Capture a beat-division-sized chunk, repeat it with optional decay and pitch shift.
**Division**: 1/4, 1/8, 1/16, 1/32 note (tempo-synced).
**Decay**: Per-repeat volume fade.
**Pitch**: Per-repeat semitone shift (via speed modulation of read position).
**Assessment**: Standard beat repeat implementation (as in Ableton's Beat Repeat or Effectrix). The pitch shift via speed modulation is the simplest approach and introduces time-stretching artifacts at extreme settings, which is part of the aesthetic.

### 8.4 DJFX Looper
**Algorithm**: Capture a beat-synced chunk, loop it with crossfade at loop point.
**Assessment**: Simple and effective loop capture.

### 8.5 Half-Speed Playback
**Algorithm**: Buffer-based speed modulation (0.5–2.0×) with crossfade on transitions.
**Assessment**: Straightforward. The crossfade on speed transitions prevents clicks.

---

## 9. BUS MIXER & PER-BUS EFFECTS

### 9.1 Bus Architecture
**File**: `effects.h:108–281`
**Buses**: 8 (4 drum + bass + lead + chord + sampler) + master.
**Per-bus chain**: Filter (SVF) → Distortion → EQ → Delay → Chorus (Classic or BBD) → Phaser → Comb → Reverb Send → Delay Send → Compressor.
**Assessment**: Professional mixer topology. Send/return architecture for reverb and delay is correct. Per-bus chorus supports both Classic and BBD modes (same algorithm as master chorus — see section 5.3). Bus BBD mode uses Hermite interpolation; bus Classic mode uses linear interpolation (cheaper, adequate for per-bus use).

### 9.2 Per-Bus Compressor
**File**: `effects.h` (bus compressor processing)
**Algorithm**: Peak envelope follower → gain computation → makeup.
**Envelope**: `alpha = 1 − exp(−dt / timeConstant)`, separate attack/release.
**Gain reduction**: `(envDb > threshold) ? excess × (1 − 1/ratio) : 0`
**Assessment**: Standard feed-forward compressor design. No soft knee on bus compressors (only on master). Adequate for bus-level dynamics control.

### 9.3 Per-Bus EQ (2-Band Shelving)
**Algorithm**: 1-pole LP split for low shelf, cascaded 1-pole for high shelf. Gain in dB, converted to linear via `pow(10, gain/20)`.
**Assessment**: Simple but effective. 1-pole shelves give 6 dB/oct slopes, which is gentle enough for broad tonal shaping. For more surgical EQ you'd want parametric bands, but for a bus-level tone control this is appropriate.

---

## 10. MASTER PROCESSING

### 10.1 Sidechain Compression (Audio Follower)
**File**: `effects.h:366–376`
**Algorithm**: Peak envelope follower with separate attack/release, applied as gain reduction to target bus.
**Bass preserve**: Optional HP filter (40–120 Hz) lets bass frequencies pass through unducked.
**Reference**: Standard sidechain compression as used in electronic music production.
**Assessment**: Correct. The bass-preserve HP is a nice feature (prevents the pumping from removing sub-bass energy).

### 10.2 Sidechain Envelope (LFOTool/Kickstart Style)
**File**: `effects.h:377–389`
**Algorithm**: Note-triggered AHR (attack-hold-release) envelope applied as ducking gain.
**Curves**: Linear, exponential (`exp(−progress×4)`), S-curve (cubic blend).
**Reference**: Models the behavior of Xfer Records LFOTool / Nicky Romero Kickstart — note-triggered volume shaping.
**Assessment**: Good. The note-triggered approach gives more consistent pumping than audio-follower sidechain (which depends on the input dynamics).

### 10.3 Multiband Processing (3-Band Split)
**File**: `effects.h:414–429`
**Algorithm**: Linkwitz-Riley crossover (2× cascaded 1-pole LP per split = 2nd order, −12 dB/oct) with allpass phase compensation.
**Bands**: Low (< lowCrossover), Mid (between), High (> highCrossover).
**Phase compensation**: Allpass filters on low/high bands to match mid band phase delay.
**Per-band**: Gain (0–2) + drive (tanh saturation when > 1).
**Reference**: Linkwitz-Riley crossover design (Linkwitz 1976). Phase-compensated multiband split described in Zölzer, "DAFX" (2011).
**Assessment**: The 2nd-order Linkwitz-Riley (LR2) crossover is the minimum order that gives flat magnitude response at the crossover frequency. Higher-order (LR4 = 4th order) would give sharper band separation. The allpass phase compensation is the correct approach — without it, the recombined signal would have phase cancellation at the crossover frequencies. This is a solid implementation for a master multiband processor.

### 10.4 Sub-Bass Boost
**File**: `effects.h:391–393`
**Algorithm**: Extract sub-bass via 1-pole LP at 60 Hz, add 0.41× (= +3 dB) back.
**Assessment**: Very simple and effective. The fixed 60 Hz frequency targets the sub-bass fundamental range. No resonance, just a gentle shelf-like boost.

### 10.5 Master EQ (2-Band Shelving)
**File**: `effects.h:395–402`
**Algorithm**: Same as per-bus EQ — 1-pole LP split for low/high shelves.
**Low**: 40–500 Hz, −12 to +12 dB. **High**: 2–16 kHz, −12 to +12 dB.
**Assessment**: Same as per-bus EQ. Adequate for broad tonal shaping.

### 10.6 Master Compressor
**File**: `effects.h:404–412`
**Algorithm**: Peak envelope follower → soft-knee gain computation → makeup gain.
**Soft knee**: Quadratic interpolation in knee zone (0–12 dB width).
**Envelope**: `alpha = 1 − exp(−dt / (timeConstant + epsilon))`.
**Gain reduction**: Hard knee: `excess × (1 − 1/ratio)` when above threshold. Soft knee: quadratic blend in transition zone.
**Makeup gain**: 0–24 dB, capped to prevent NaN/inf.
**Reference**: Standard feed-forward compressor with soft knee (Giannoulis et al., "Digital Dynamic Range Compressor Design — A Tutorial and Analysis", 2012).
**Assessment**: Good quality master compressor. The soft knee option is important for transparent mastering-style compression. The gain cap at +24 dB prevents numerical issues. The envelope follower uses proper exponential coefficients. For critical mastering use, you might want RMS detection (currently peak), lookahead, and a sidechain HP filter, but for real-time game audio this is well-suited.

---

## 11. UTILITY DSP

### 11.1 Cubic Hermite Interpolation
**File**: `synth.h:1473+`
**Algorithm**: 4-point Catmull-Rom spline. `a = −0.5·y0 + 1.5·y1 − 1.5·y2 + 0.5·y3; b = y0 − 2.5·y1 + 2·y2 − 0.5·y3; c = −0.5·y0 + 0.5·y2; d = y1; output = ((a·t + b)·t + c)·t + d`
**Reference**: Catmull & Rom (1974). Standard for audio interpolation.
**Used by**: SCW wavetable, granular, delay lines, dub loop, chorus.
**Assessment**: The right choice for audio. Much less HF rolloff than linear, much cheaper than sinc.

### 11.2 DC Blocking
**File**: `synth_oscillators.h:328–332` (pipe oscillator)
**Algorithm**: `dcOut = out − dcPrev + 0.995·dcState` — 1st-order highpass at ~3.5 Hz.
**Reference**: Standard DC blocker (Julius O. Smith, "Introduction to Digital Filters").
**Assessment**: Correct. The 0.995 coefficient gives a −3 dB point well below audible range.

### 11.3 Denormal Flushing
**File**: `synth.h:843–845` (ladder filter)
**Algorithm**: `if (|s[i]| < 1e-15) s[i] = 0` — explicit flush to zero for very small values.
**Reference**: Standard technique to prevent denormalized floating-point numbers from causing CPU performance degradation on x86.
**Assessment**: Correct. The 1e-15 threshold is well below the audio noise floor.

---

## 12. SUMMARY TABLE

| Component | Algorithm | Quality | Key Reference |
|-----------|-----------|---------|--------------|
| Square/Saw/Tri | PolyBLEP | Good | Välimäki 2005 |
| Noise | LCG | Adequate | Knuth |
| Wavetable (SCW) | Hermite interp | Good | Roads 1996 |
| Formant (Voice) | Source-filter + Chamberlin SVF | Good | Klatt 1980, Chamberlin 1985 |
| Plucked String | Karplus-Strong + Thiran allpass | Very good | Karplus & Strong 1983, Jaffe & Smith 1983 |
| Bowed String | Smith waveguide + friction | Good | Smith PASP, McIntyre 1983 |
| Blown Pipe | Fletcher/Verge jet-drive | Good | STK Flute, Cook 2002 |
| Additive | Sine bank | Good | Risset & Mathews 1969 |
| Mallet | Modal (4 modes) | Good | Fletcher & Rossing 1998 |
| Granular | Async Hanning grains | Good | Roads 2001 |
| FM | 2/3-op Chowning FM | Good | Chowning 1973 |
| Phase Distortion | CZ-style phase warp | Good | Casio CZ-101 (1984) |
| Membrane | Modal (6 Bessel modes) | Good | Fletcher & Rossing 1998, Morse & Ingard 1968 |
| Bird | Parametric chirp | Adequate | Procedural |
| SVF Filter | Simper/Cytomic TPT | Excellent | Simper 2013, Zavalishin 2012 |
| Ladder Filter | 4-pole TPT + 2× OS, true multimode | Excellent | KR-106, Zavalishin 2012 |
| One-pole Filter | 1st-order IIR | Adequate | Textbook |
| Reso Bandpass | Chamberlin feedback | Adequate | Chamberlin 1985 |
| Reverb (Schroeder) | 4 combs + 2 allpass | Adequate | Schroeder 1962, Moorer 1979 |
| Reverb (FDN) | 8-line Hadamard FDN + early ref | Very good | Jot 1992, Schlecht & Habets 2017 |
| Analog Variance | Deterministic per-voice hash | Good | Prophet-5/Juno tolerances |
| Chorus (Classic) | Dual-LFO modulated delay | Good | Standard |
| Chorus (BBD) | Antiphase triangle + charge sat | Very good | Juno-6 BBD circuit, Raffel & Smith 2010 |
| Flanger | Short mod delay + feedback | Good | Standard |
| Phaser | Allpass cascade + LFO | Good | Standard |
| Delay | Hermite-interp circular buffer | Good | Standard |
| Dub Loop | Multi-head tape + degradation | Very good | Original (King Tubby inspired) |
| Compressor | Peak envelope + soft knee | Good | Giannoulis 2012 |
| Multiband | LR2 crossover + phase comp | Good | Linkwitz 1976 |
| 303 Acid | RC discharge + gimmick dip | Very good | Whittle TB-303 analysis |
| Sidechain | Peak follower / note envelope | Good | Standard |

### Areas where upgrades would yield the most improvement:
1. **Wavefolding** — No oversampling, so aliasing at high fold amounts. 2× OS would help.
2. **Formant filters** — Chamberlin SVF could be upgraded to Cytomic SVF for better HF accuracy (not critical since formants are < 3 kHz).
3. **Tape wow/flutter** — Sine LFOs are periodic. Noise-modulated LFOs would give more realistic irregularity.
4. **Reverb sample rate scaling** — Both Schroeder and FDN delay lengths are hardcoded for 44100 Hz. At other sample rates, lengths should be scaled proportionally.
