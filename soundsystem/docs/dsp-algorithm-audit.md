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

### 1.15 Pure Sine (WAVE_SINE)
**File**: `synth.h` (main oscillator switch)
**Algorithm**: `sinf(phase * 2π)` — mathematically pure sine wave, no harmonics.
**Unison**: Supports multi-oscillator unison (up to 4 voices with detune), same as square/saw/triangle.
**Reference**: The fundamental building block of all sound. Fourier's theorem.
**Assessment**: Added to complete the basic waveform set (square, saw, triangle, noise, sine). Previously sine was only available indirectly via FM with zero modulation index or additive with a single harmonic. Essential for Casio MT-70 style additive patches where the base timbre must be a clean sine, and for sub-bass, test tones, and simple organ/flute sounds.

### 1.16 Unison / Multi-Oscillator
**File**: `synth.h:2114–2153`
**Algorithm**: Up to 6 extra oscillators at configurable frequency ratios, levels, and independent per-oscillator decay envelopes.
**Mix modes**: Weighted average (default) or additive sum (for metallic hihat/cowbell sounds).
**Per-oscillator decay**: Each extra osc (osc2–osc6) has a `decay` parameter (0 = sustain with main envelope, >0 = independent exponential decay rate). On note trigger, the per-osc envelope starts at 1.0 and decays via `env *= 1 - decay/sampleRate`. This enables timbral evolution where overtones fade independently of the fundamental — the core technique behind Casio MT-70 (1982) additive sine synthesis and similar early digital keyboards.
**UI**: Semitone +/- buttons on ratio knobs allow quick interval-based tuning (left-click = +1 semitone, right-click = -1 semitone). The ratio value is multiplied/divided by `2^(1/12)`.
**Reference**: Standard detuned unison technique used in virtually all analog and VA synths. The per-oscillator decay is conceptually similar to how early Casio/Yamaha digital keyboards shaped timbres — each partial had its own amplitude envelope, typically a simple exponential decay.
**Velocity modulation system** (4 targets, all using squared curve `vel²` for natural response):
- `p_oscVelSens` (0–5): Scales extra osc levels. Formula: `level *= 1 - sens + sens * vel²`. Useful for patches where upper partials should respond to dynamics. Note: for Rhodes, bell partials should NOT be velocity-scaled (they're always present from tine physics) — use velToDrive instead for the bark.
- `p_velToFilter` (0–5): Adds to filter cutoff. Formula: `cutoff += amount * vel²`. Soft = darker, hard = brighter.
- `p_velToClick` (0–5): Scales click/hammer level. Formula: `click *= 1 - amount + amount * vel²`. Soft = no thump, hard = pronounced hammer.
- `p_velToDrive` (0–5): Adds to drive amount. Formula: `drive += amount * vel²`. This is the key to Rhodes bark — pickup nonlinearity is amplitude-dependent, so only hard hits should saturate. Values > 1.0 needed for audible DIST_ASYMMETRIC effect.
**Squared curve rationale**: Linear velocity (vel) gives too little contrast — at half velocity, partials are still at 50%. Squared (vel²) gives soft=6%, mid=25%, hard=100%, matching the exponential response of real acoustic instruments where harmonics grow nonlinearly with excitation force.
**Cyan UI indicators**: Real-time visualization on affected parameter rows (filter cutoff, drive, click level, extra osc levels) showing the velocity-modulated value, matching the LFO orange indicator style.
**Assessment**: Straightforward. The detuning is ratio-based (not cents-based), which is fine for the metallic percussion use case. For typical unison-lead sounds, a cents-based spread with even distribution (as described in the unison globals) would be more musical. Both modes are available. The per-oscillator decay + velocity modulation enable rich dynamic timbres: MT-70 style additive (static partials with independent decay), Rhodes (always-present bell + velocity-driven bark), and anything in between.

### 1.17 Electric Piano (WAVE_EPIANO)
**File**: `synth_oscillators.h`, init at `initEPianoSettings()`, process at `processEPianoOscillator()`
**Algorithm**: 12-mode modal synthesis with 3 pickup types (Rhodes/Wurlitzer/Clavinet), per-type nonlinearity with register scaling, and DC blocker. Tuned against real instrument samples.

**Physics model**: Three instruments share a common 12-mode modal engine, fully branched per pickup type for register scaling, velocity curves, pickup nonlinearity, and decay:

| | Rhodes (1965) | Wurlitzer 200A (1954) | Clavinet D6 (1971) |
|---|---|---|---|
| **Exciter** | Hammer → tine + tone bar | Hammer → steel reed | Tangent → string + damper pad |
| **Resonator** | Cantilever beam (tuning fork) | Clamped-free reed | Short steel string |
| **Pickup** | Electromagnetic (asymmetric) | Electrostatic (symmetric) | Contact/magnetic (mixed) |
| **Modes used** | 12 (upper modes feed pickup intermod) | 6 (modes 7-12 zeroed) | 6 (modes 7-12 zeroed) |
| **Character** | Bell attack + warm sustain | Reedy/nasal, odd harmonics | Bright, percussive, funky |

Pickup type selected via `epPickupType`: `EP_PICKUP_ELECTROMAGNETIC` (0), `EP_PICKUP_ELECTROSTATIC` (1), `EP_PICKUP_CONTACT` (2).

#### Rhodes (electromagnetic)

**Physics**: Hammer strikes a tine (clamped-free cantilever beam) coupled to a brass tone bar, vibrating near an electromagnetic pickup.

**Ratio sets** (`p_epRatioSet`, UI toggle "Beam" vs "Tine"):
- **Set 0 (Beam)**: Euler-Bernoulli eigenvalues `{1, 6.27, 17.55, 34.39, 56.84, 84.91, 120, 162, 211, 267, 330, 400}` — pure theory, wildly inharmonic upper partials.
- **Set 1 (Tine+spring)**: Measured ratios `{1, 4.2, 9.5, 16.3, 24.8, 35, 47, 61, 77, 95, 115, 137}` — real Rhodes tines are tuned by a spring that pulls upper modes closer to harmonic (Shear 2011, Gabrielli 2020). **Default for all Rhodes presets.**

**Mode ratio blending**: `epBellTone` blends between harmonic ratios `{1, 2, 3, ..., 12}` and the selected inharmonic set. Bell blend is **register-scaled**: `btEff = bellTone × freqNorm²` — quadratic, so low register is nearly harmonic (warm piano-like) while high register gets bell character. This was tuned against real Rhodes samples where B1 has inharmonicity 0.02 (nearly pure) while B4 has 0.14.

Per-mode blend scale (tine set): `{0, 0, 0, 0.5, 0.8, 1, 1, 1, 1, 1, 1, 1}` — body modes (1-3) locked to harmonic ratios, bell modes (4+) get inharmonicity. This prevents detuning when sweeping bellTone.

**Pickup position profiles** — fundamental-heavy, letting the pickup nonlinearity generate harmonics:
- Centered: `{1.0, 0.04, 0.03, 0.06, 0.03, 0.02, 0.015, 0.012, 0.010, 0.008, 0.006, 0.004}`
- Offset: `{0.60, 0.35, 0.08, 0.20, 0.08, 0.05, 0.04, 0.03, 0.025, 0.02, 0.015, 0.01}`

The 2nd partial is deliberately low in the modal bank (0.04 centered) because the pickup `sum²` nonlinearity generates strong 2nd harmonics. Earlier profiles had the 2nd partial at 0.55, which combined with pickup distortion produced +25dB excess at the 2nd harmonic vs real Rhodes samples.

**Register scaling** (all upper modes): Cubic/6th-power rolloff with frequency:
- Body modes (1-2): `(1 - freqNorm)³ × 0.95` — gentle in playing range, near-zero at top octave
- Bell modes (3+): `(1 - freqNorm)⁶` — extremely steep, zero at D6+

Real Rhodes top octave (D6-C7) is nearly a pure sine — measured samples show only fundamental with <1% 2nd partial. The steep rolloff matches this.

**Velocity scaling** (Rhodes-specific):
- Fundamental: floor 0.15, linear+vel² blend
- 2nd partial: floor 0.20, linear-dominant — always audible for warmth
- 3rd partial: floor 0.15, balanced blend
- Bell modes (4+): floor 0.08, vel² curve — quiet at pp, present at ff

**Pickup nonlinearity** — register-scaled (`regDist = (1 - freqNorm)²`):
- `sum²` (even harmonics): `k = (0.25 + pickupDist × 1.2 × velBoost) × regDist`
- `sum³` (odd + intermod): `k2 = (0.20 + pickupDist × 0.5 × velBoost) × regDist`
- `sum⁵` (upper harmonics): `k3 = pickupDist × 0.3 × velBoost × regDist`
- Asymmetric soft-clip with register-scaled drive: `tanh(x × clipDrive)` positive, `tanh(x × clipDrive × 0.85) × 0.9` negative

The register scaling is critical: low register tines are long, drive the pickup hard (regDist≈1.0), generating rich harmonics up to the 18th partial. High register tines are short, barely excite the pickup (regDist≈0.04 at D6), output approaches pure sine — matching real Rhodes measurements.

**Normalization**: `target = 0.15 + 0.85 × vel^1.5` — compressed curve with floor. The floor ensures even pp hits produce enough signal to drive the pickup into generating some even harmonics (without it, vel=0.15 gives target=0.058, too quiet for audible pickup distortion).

**Tone bar coupling** (Rhodes only): Extends fundamental decay by up to 2.5× and 2nd partial by up to 1.6×.

**Decay** (Rhodes-specific): `decayFactor = 1 / (1 + (ratio-1) × (0.2 + btEff × 0.5))`, register scaling `epDecay × (1 - freqNorm × 0.35)` — gentler than Wurli/Clav.

#### Wurlitzer (electrostatic)

**Physics**: Hammer strikes a steel reed vibrating between two charged plates (electrostatic pickup). The symmetric capacitive geometry cancels even modes partially, producing a strong odd-harmonic series (clarinet-like).

**Mode ratios**: `{1, 2.02, 3.01, 5.04, 7.05, 9.08}` (modes 7-12 zeroed). Note the gap at the 4th partial (jumps to 5.04), emphasizing odd harmonics.

**Pickup position profiles** (tuned against real Wurlitzer 200A samples):
- Centered: `{1.0, 0.08, 0.45, 0.12, 0.10, 0.04}` — 3rd harmonic dominant, moderate 4th
- Offset: `{0.60, 0.15, 0.60, 0.20, 0.20, 0.08}` — buzzy, stronger upper partials

Measured C4 partials from real Wurli 200A: fund=100%, 2nd=9%, 3rd=42%, 4th=15%, 5th=6%, 6th=6%. Earlier profiles had mode 5 at 0.35 (vs real 6.3%), producing an overly buzzy sound.

**Register scaling** (Wurli-specific): Quadratic thinning for body modes, 4th-power for upper modes. Real Wurli 200A at C6 is nearly fundamental-only; at A6 it's a pure sine.

**Pickup nonlinearity** — register-scaled (`regDist = (1 - freqNorm)²`):
- Symmetric odd-order: `sum³` (`k3 = pickupDist × 1.5 × velBoost × regDist`) + `sum⁵` (`k5 = pickupDist × 0.4 × velBoost × regDist`)
- Symmetric `tanh` soft-clip (no asymmetry)

**Velocity scaling** (Wurli-specific): 3rd partial has higher floor (0.12) — always present as the defining Wurli character. Upper modes use vel² with low floor (0.05).

**Decay**: Original curves — `decayFactor = 1 / (1 + (ratio-1) × (0.3 + btEff × 0.7))`, register scaling 50%.

**No tone bar coupling** — reeds decay naturally without reinforcement.

#### Clavinet (contact)

**Physics**: A tangent strikes a short steel string from below; a yarn-wound damper pad releases, producing the characteristic "twang." Two single-coil magnetic pickups under the strings (neck/bridge, like a guitar). Nearly harmonic — stiff string inharmonicity is subtle.

**Mode ratios**: `{1, 2.003, 3.012, 4.028, 5.15, 6.35}` (modes 7-12 zeroed). Blend scale `{0, 0, 0.05, 0.2, 0.5, 0.75, 0.85, 0.9, 0.95, 1, 1, 1}`.

**Pickup profiles**: Neck/centered `{1.0, 0.30, 0.20, 0.35, 0.15, 0.06}` — warm. Bridge/offset `{0.60, 0.55, 0.50, 0.20, 0.30, 0.10}` — bright funk sound.

**Pickup nonlinearity**: Mixed even+odd: `sum²` (k2 = pickupDist × 0.8 × velBoost) + `sum³` (k3 = pickupDist × 1.0 × velBoost). Symmetric clip `tanh(output × 1.2)`. **Not register-scaled** (no reference samples for validation).

**Velocity/register**: Original curves (not yet tuned against reference samples).

#### Common engine details

**DC blocker**: `y[n] = x[n] − x[n−1] + 0.995 · y[n−1]` (~7 Hz cutoff at 44.1 kHz). Essential — the `sum²` nonlinearity generates DC offset proportional to signal power.

**Phase accumulation**: `fmodf(phase, 1.0)` instead of `phase -= floorf(phase)` — avoids float precision loss on long sustains.

**Presets** (9):
| # | Name | Pickup | Character | Key settings |
|---|------|--------|-----------|--------------|
| 174 | Rhodes Warm | EM | Classic mellow — ballads, neo-soul | centered, soft hammer, drive=0.44, velToDrive=3.0, tine ratios |
| 175 | Rhodes Bright | EM | Funky, cutting — comps, riffs | far offset, hard hammer, drive=0.55, velToDrive=2.5, vel→filter wah |
| 176 | Rhodes Suite | EM | Dreamy suitcase — pads, slow ballads | very soft hammer, max tone bar, long release, tremolo 4.5Hz |
| 177 | Rhodes Bark | EM | Glass bell → savage growl | zero drive at pp, velToDrive=5.0, vel→filter+click, pickupDist=0.7 |
| 178 | Wurli Buzz | ES | Supertramp "Dreamer" | driven, nasal, tremolo |
| 179 | Wurli Soul | ES | Ray Charles ballad | warm, clean, gentle tremolo |
| 180 | Clav Funky | CT | Stevie Wonder funk | bridge pickup, wah filter, hard attack |
| 181 | Clav Mellow | CT | Bill Withers / reggae | neck pickup, warm, soft touch |
| 182 | Clav Driven | CT | Led Zeppelin rock | both pickups, heavy distortion |

**Reference comparison** (preset-audition vs real instrument samples):

Rhodes Warm (174) vs real Rhodes samples (multi-velocity, full register):

| Register | Spectrum match | Key finding |
|----------|---------------|-------------|
| G3-D4 (mid) | **98-99%** | Near-perfect harmonic content |
| B4 | **95%** | Mode 2 within 2.5dB of reference |
| B1 (low) | **91-99%** | Harmonic at low register (inharm=0.00), rich from pickup |
| D6-C7 (top) | 65% | 2-3% residual 2nd partial (real Rhodes is pure sine) |

Wurlitzer (111) vs real Wurlitzer 200A samples:

| Register | Spectrum match | Key finding |
|----------|---------------|-------------|
| A4 | **90-93%** | Good match across velocities |
| C6 | **95-97%** | Register scaling working well |
| C2 (low) soft | **92%** | Solid |
| C4 (mid) | 60-63% | Mode balance needs further preset tuning |

**Reference**:
- Shear & Wright, "The Electromagnetically Sustained Rhodes Piano" (UCSB Masters Thesis, 2011) — Q measurements, spectral analysis, tine dimensions
- Pfeifle, "Real-time Physical Model of a Wurlitzer and Rhodes Electronic Piano" (DAFx-17, 2017) — beam model, transient analysis
- Gabrielli, Cantarini, Castellini & Squartini, "The Rhodes electric piano: Analysis and simulation of the inharmonic overtones" (JASA 2020) — scanning laser vibrometry on actual tines
- Pfeifle & Muenster, "Tone Production of the Wurlitzer and Rhodes E-Pianos" (DAGA/Springer 2017)
- Fletcher & Rossing, "The Physics of Musical Instruments" (1998), Ch. 2 (beams), Ch. 12 (electric instruments)
- Sound On Sound, "Synthesizing Pianos" (Gordon Reid, 2001)
- Sound On Sound, "Synth Secrets: Clavinet" (Gordon Reid, 2002) — Clavinet D6 mechanics and synthesis
- Real Rhodes samples (multi-velocity, B1-C7) — used for per-partial spectral matching
- Real Wurlitzer 200A samples (Pie-finger sample set, 3 velocity layers, C2-A6) — used for register/velocity tuning

**Assessment**: Good semi-physical model with three fully distinct pickup characters. Each instrument has its own register scaling, velocity curves, pickup nonlinearity, and decay model — branched per pickup type, not shared. Key design decisions validated against real samples: (1) Rhodes modal bank is fundamental-heavy (2nd partial at 0.04), letting the pickup nonlinearity generate even harmonics — prevents the +25dB 2nd-partial excess the old 0.55 profile produced; (2) Register-scaled pickup distortion (quadratic falloff) matches the real instrument: rich harmonics at low register, pure sine at top octave; (3) Tine+spring ratio set (measured) replaces Euler-Bernoulli beam theory for more musical bell character; (4) 12 modes for Rhodes provide material for pickup intermodulation to generate the rich 18-partial harmonic series measured in real low-register samples; (5) Wurli mode 5 reduced from 0.35→0.10 to match measured 6.3% at C4. **Possible enhancements**: (1) Release damper thump on note-off. (2) Sympathetic resonance between adjacent tines. (3) Clavinet reference sample comparison (no samples available yet).

### 1.18 Tonewheel Organ / Hammond B3 (WAVE_ORGAN)
**File**: `synth_oscillators.h:1872–1980`, init at `1872–1897`, process at `1899–1980`
**Algorithm**: 9-drawbar additive synthesis (sine bank at Hammond ratios) + key click + single-trigger percussion + tonewheel crosstalk + scanner vibrato/chorus.

**Physics model**: The Hammond B3 (1955) uses 91 spinning tonewheels (electromagnetic tone generators) to produce near-pure sine waves. 9 drawbars mix specific harmonics at adjustable levels, routed through a preamp and typically a Leslie rotary speaker cabinet. The engine models four distinct components of the Hammond sound:

**Drawbar synthesis**: 9 sine oscillators at non-standard Hammond ratios:
```
Drawbar  Footage  Ratio   Interval
  1       16'     0.5     Sub-octave
  2       5⅓'    1.5     Fifth above sub (= 3rd harmonic of 16')
  3       8'      1.0     Fundamental (unison)
  4       4'      2.0     Octave
  5       2⅔'    3.0     Octave + fifth
  6       2'      4.0     Two octaves
  7       1⅗'    5.0     Two octaves + major third
  8       1⅓'    6.0     Two octaves + fifth
  9       1'      8.0     Three octaves
```
Each drawbar level is 0–1 (maps from the Hammond's 0–8 integer scale). Drawbar 7 (1⅗', the major 3rd) is the "color" drawbar that gives Hammond its unique harmonic flavor vs a simple additive organ. Phase accumulators use `fmodf` for precision on long sustains. Output is normalized by the sum of active drawbar levels to prevent clipping when many drawbars are engaged.

**Key click**: On note-on, a velocity-scaled noise burst (`noise() × clickEnv × 0.5`) with ~3ms exponential decay (`clickEnv *= 1 − dt/0.003`). Models the contact bounce when a Hammond key closes its 9 busbars. Real Hammonds had varying amounts of click depending on key condition — players often preferred more click (the B3 redesigns that eliminated it were unpopular). Amount controllable via `orgClick` (0–1).

**Percussion**: A fast-decaying sine at the 2nd or 3rd harmonic, triggered on note attack. Critical Hammond feature with unique behavior:
- **Single-trigger**: Only the first note in a chord gets percussion. Tracked via global `orgPercKeysHeld` counter — percussion fires when count is 0, incremented on note-on, decremented on note-off.
- **Non-retriggerable**: Must release all keys before percussion fires again (the Hammond percussion circuit shared a single capacitor across the entire keyboard).
- Fast decay (~200ms) or slow decay (~500ms), soft (−3dB) or normal volume. `percEnv *= 1 − dt/percDecay`.

**Tonewheel crosstalk**: Adds subtle leakage from adjacent tonewheels (~−36dB): `sin((phase + 0.037) × 2π) × level × crosstalk × 0.015`. The 0.037 phase offset models the physical proximity of adjacent wheels on the shared shaft. This is what makes a vintage Hammond sound "alive" vs a sterile additive synth — the real instruments had measurable crosstalk between wheels sharing the same tone generator.

**Scanner vibrato/chorus** (Hammond V/C system): Per-voice delay-line effect modeled after the Hammond's unique mechanical scanner — a rotating capacitor that sweeps across 9 taps of an LC delay line. Implementation: 64-sample circular buffer (~1.45ms at 44100 Hz) with a 6.9 Hz sine LFO (locked to the tonewheel motor: 60 Hz mains ÷ 8.7 gear ratio) modulating the read position around a center tap at 32 samples. 6 modes:
- V1/C1: ±6.6 samples (±0.15ms) — subtle shimmer
- V2/C2: ±17.6 samples (±0.40ms) — classic warm chorus
- V3/C3: ±30.9 samples (±0.70ms) — full dramatic vibrato

V modes output wet signal only (pitch vibrato). C modes output 50% dry + 50% wet (chorus — the phase difference creates frequency-dependent cancellation/reinforcement, giving the characteristic swirling depth). C3 is *the* classic Hammond jazz/gospel sound. Read position uses `cubicHermite` interpolation for smooth pitch modulation without aliasing.

**Signal chain**: Drawbar sum → Scanner V/C → Key click + Percussion → (external: Leslie rotary speaker, bus effects)

**Presets** (9):
| # | Name | Drawbars | V/C | Character |
|---|------|----------|-----|-----------|
| 183 | Jimmy Smith | 888000000 | C3 | Fat jazz, full chorus |
| 184 | Gospel Full | 888888888 | C3 | All drawbars, massive choir |
| 185 | Jon Lord Rock | 888600000 | V3 | Deep Purple growl |
| 186 | Booker T Green | 886000000 | C1 | Green Onions, subtle |
| 187 | Organ Ballad | 808000004 | C2 | Sub + fund + shimmer |
| 188 | Reggae Bubble | 006060000 | Off | Tight staccato skank |
| 189 | Larry Young | 888800000 | C2 | Unity-era modern jazz |
| 190 | Emerson Prog | 888808008 | V3 | ELP bombast |
| 191 | Soft Combo | 006600400 | C1 | Cocktail lounge |

**Reference**:
- Smith & Abel, "Closed-Form Swept Sine Delay for Leslie Speaker" (ICMC 1999) — scanner delay analysis
- Pekonen, Pihlajamäki & Välimäki, "Computationally Efficient Hammond Organ Synthesis" (DAFx 2011) — tonewheel model, crosstalk measurements
- Werner & Smith, "The Leslie Speaker Cabinet" (DAFx 2016) — Leslie + scanner interaction
- Pakarinen & Välimäki, "Physical Modeling of the Hammond Organ Vibrato/Chorus" (DAFx 2009) — scanner delay line analysis, LC line measurements
- Hammond B3 Service Manual (1955) — tonewheel specifications, drawbar ratios, percussion circuit schematic

**Assessment**: Good additive model with authentic Hammond-specific features. The drawbar ratios are exact Hammond B3 values (not approximations). The key click noise burst is a reasonable stand-in for the actual busbar contact bounce (a more accurate model would use a burst of all 9 drawbar harmonics at random phases, but noise is perceptually close). Single-trigger percussion correctly models the shared capacitor circuit. The scanner vibrato/chorus uses a delay-line approach (Approach A from Pakarinen 2009) which naturally produces frequency-dependent phase shifts — higher harmonics accumulate more phase shift per sample of delay, creating the characteristic "swirling" timbre that distinguishes Hammond V/C from plain pitch vibrato. **Possible enhancements**: (1) Preamp overdrive — tube-style even-harmonic distortion scaled by drawbar sum (more drawbars = hotter signal = more natural drive). (2) Transistor organ modes (Vox Continental, Farfisa) — square/pulse waveforms with tab-stop filtering instead of sine drawbars. (3) Tonewheel wear — subtle per-wheel frequency drift and harmonic distortion for a more "played-in" character. (4) Scanner vibrato LFO shape — rounded triangle (closer to real scanner scan pattern) instead of pure sine.

### 1.19 Metallic Percussion (WAVE_METALLIC)
**File**: `synth_oscillators.h:1347–1811`, init at `1383–1639`, process at `1641–1811`
**Algorithm**: 6 square/sine oscillators arranged as 3 ring-modulated pairs, with per-partial exponential decay, HP-filtered noise layer, and pitch envelope.

**Core topology** (authentic 808/909 hihat):
```
Pair 0: osc[0] × osc[1]  ──┐
Pair 1: osc[2] × osc[3]  ──┼── ringOut (weighted sum)
Pair 2: osc[4] × osc[5]  ──┘
                              ──→ ringMix crossfade ──→ + HP noise ──→ output
osc[0] + osc[1] + ... + osc[5] ──→ addOut
```

**Ring modulation**: Each pair multiplied together produces sum and difference frequencies — `cos(A)·cos(B) = ½[cos(A−B) + cos(A+B)]`. With 6 oscillators at inharmonic ratios, the 3 pairs generate 6 sum/difference tones that are not harmonically related to the fundamental, creating the characteristic metallic "clang" that distinguishes real 808 hihats from simple additive mixing.

**808 ratios**: `{1.0, 1.4471, 1.617, 1.9265, 2.5028, 2.6637}` — from the Roland TR-808 service notes. The 808 uses 6 metal-square oscillators at these ratios, ring-modulated in pairs through bridged-T bandpass circuits.

**909 ratios**: `{1.0, 1.4953, 1.6388, 1.9533, 2.5316, 2.7074}` — slightly different from 808, producing a brighter, more brittle character with more harmonic spread.

**Brightness**: Crossfade between sine and square per oscillator (`sine + brightness × (square − sine)`). Sine modes (brightness=0) produce pure bell/chime tones; square modes (brightness=1) produce the harsh metallic character of the 808.

**Per-partial decay**: Each of the 6 oscillators decays independently: `modeAmps[i] *= (1 − dt / modeDecays[i])`. Higher partials decay faster than lower ones, so the sound evolves from bright initial "tss" to darker sustain "shh" — matching real cymbal behavior where high-frequency modes radiate energy faster.

**Pitch envelope**: `pitchMult = 1 + (semitones/12) × exp(−t / decay)`. A small pitch drop in the first few milliseconds gives the sharp attack transient "tick" that makes 808 hihats cut through a mix.

**HP noise layer**: White noise via `noise()` → 1-pole HP filter (subtraction from LP state) at configurable cutoff. Adds sizzle/air to cymbals. Amount controllable per preset (0.15 for 808, 0.25–0.30 for 909, 0 for cowbell/bell).

**Mix formula**: `out = addOut + ringMix × (ringOut − addOut)`. At ringMix=0, output is pure additive (same as the old WAVE_SQUARE-based hihat presets). At ringMix=0.85–0.9, output is predominantly ring-modulated (authentic 808/909).

**Presets** (11):
| # | Preset | Ratios | Ring | Noise | Bright | Character |
|---|--------|--------|------|-------|--------|-----------|
| 808 CH | 808 | 0.85 | 0.15 | 1.0 | Tight sizzle, choke-able |
| 808 OH | 808 | 0.85 | 0.12 | 1.0 | Same character, longer ring |
| 909 CH | 909 | 0.9 | 0.25 | 1.0 | Brighter, more noise |
| 909 OH | 909 | 0.9 | 0.3 | 1.0 | Bright, washy |
| Ride | Wide spread | 0.7 | 0.08 | 0.7 | Long shimmer, subtle |
| Crash | Wide spread | 0.75 | 0.2 | 0.8 | Bright burst, very long tail |
| Cowbell | 1:1.504 | 0.0 | 0.0 | 1.0 | Two-tone, additive (1 pair only) |
| Bell | Near-harmonic | 0.3 | 0.0 | 0.0 | Tubular bell, sine modes |
| Gong | Low cluster | 0.5 | 0.05 | 0.2 | Very long decay, pitch drop |
| Agogo | 1:2.8:4.1 | 0.4 | 0.0 | 0.5 | Bright bell, 3 modes only |
| Triangle | 1:2.76:5.4 | 0.2 | 0.02 | 0.0 | Almost pure sine, very long ring |

**Reference**:
- Roland TR-808 Service Manual (1980) — oscillator circuit, frequency ratios, HP filter topology
- Roland TR-909 Service Manual (1983) — modified oscillator ratios, noise mixer
- Bilbao, "Numerical Sound Synthesis" (2009) — modal synthesis of metallic percussion
- Fletcher & Rossing, "The Physics of Musical Instruments" (1998), Ch. 20 (cymbals and gongs) — Bessel/circular plate mode analysis, decay rate vs. frequency relationship

**Assessment**: Significant improvement over the previous WAVE_SQUARE additive approach for hihats. The ring modulation is the key — additive mixing of inharmonic squares produces a buzzy chord, while ring-mod produces the actual sum/difference frequency spectrum that defines the 808 sound. The per-partial decay adds the timbral evolution that real cymbals exhibit. The brightness control enables the full range from pure sine bells to harsh metallic hats in one engine. 6 oscillators is the correct count for 808/909 (matching the original hardware). For cymbals, real instruments have hundreds of modes — 6 is a deliberate simplification that captures the essential character. **Possible enhancements**: (1) Choke envelope — a fast amplitude kill triggered by a subsequent closed hihat, with a brief "zzip" transient. Currently choke is handled by the voice system's `p_choke` flag, which is adequate. (2) Velocity-to-brightness mapping for natural dynamics.

### 1.20 Guitar Body (WAVE_GUITAR)
**File**: `synth_oscillators.h:1817–2060`, init at `1849–2015`, process at `2017–2060`
**Algorithm**: Karplus-Strong plucked string (reusing WAVE_PLUCK infrastructure) → pick position comb filter → sitar-style bridge buzz → 4 parallel biquad body resonators → dry/wet mix.

**Signal flow**:
```
Noise burst ──→ pick position comb ──→ KS delay line (string)
                                            │
                                     ┌──────┤
                                     │      │
                              KS LP filter (per-preset brightness/damping)
                                     │
                              jawari bridge (amplitude-dependent delay mod)
                                     │
                              DC blocker (1st-order HP ~3.5 Hz)
                                     │
                                     └──→ write back to delay line
                                            │
                                            ├──→ 4× biquad BPF (body) ──→ bodyMix
                                            │                                │
                                            └──→ dry string ────────────────→ output
```

**String model**: KS delay line with **per-preset damping and brightness** that override the global pluck parameters. Each preset has its own `stringDamping` (0.9965–0.9995, controls sustain length) and `stringBrightness` (0.20–0.85, controls the LP filter's harmonic rolloff). This is the primary source of timbral differentiation — a koto (bright=0.85, long ring) sounds fundamentally different from an oud (bright=0.20, short decay) even before body resonance.

**DC blocker**: A 1st-order highpass filter (`y[n] = x[n] − x[n−1] + 0.995·y[n−1]`, cutoff ~3.5 Hz) inside the KS feedback loop. This prevents DC offset accumulation on long-sustain presets (especially harp at damping=0.9995) which would otherwise saturate the effects chain and cause silence when multiple notes are held.

**Pick position** (excitation comb filter): Applied to the initial noise burst at note-on. Creates a comb-notch at `sampleRate / (pickPos × ksLength)` and its harmonics. Near bridge (pickPos=0): all harmonics present → bright, twangy. Near center (pickPos=1): notch suppresses harmonics near the midpoint → warm, round. Implementation: `ksBuffer[i] = (noise[i] + noise[(i + pickSample) % len]) × 0.5` — a feedforward comb filter that notches frequencies where the pick position coincides with a vibration node.

**Reference**: Smith, "Physical Audio Signal Processing" — the pick position comb filter is the standard technique for modeling where a string is plucked. This is equivalent to the spatial impulse response at the pluck point: a delta function convolved with its reflection from the nearest boundary.

**Bridge buzz** (jawari — amplitude-dependent delay modulation): Models the sitar's curved parabolic bridge by modulating the KS allpass fractional delay coefficient based on signal amplitude. When the string vibrates with large amplitude, it wraps around the bridge surface, shortening the effective vibrating length (raising pitch). As the note decays, the string lifts off → pitch drops back to nominal. This creates the characteristic **descending "precursor"** — a burst of high harmonics that sweeps downward during decay.

Implementation: `apCoeff = baseCoeff × (1 − amplitude² × buzzAmount × 1.5)`. The squared amplitude models the parabolic bridge profile. The modulated coefficient changes the fractional delay, which is equivalent to time-varying string length. This is the physically correct mechanism (van Walstijn & Bridges 2016), unlike the earlier tanh soft-clipping approach which sounded like distortion rather than a real jawari.

**Reference**: van Walstijn & Bridges, "A Real-Time Synthesis Oriented Tanpura Model" (2016) — amplitude-dependent string length modulation. Siddiq, "A Physical Model of the Nonlinear Sitar String" (2012). Smith, "Physical Audio Signal Processing" (CCRMA) — string length modulation.

**Body resonator**: 4 parallel biquad bandpass filters, each tuned to a specific body formant frequency. The biquads use the standard cookbook formula:
```
w0 = 2π × freq / sr
alpha = sin(w0) × sinh(ln(2)/2 × BW × w0 / sin(w0))
b0 = alpha × gain / a0
b2 = −alpha × gain / a0
a1 = −2cos(w0) / a0
a2 = (1 − alpha) / a0
```
Transposed direct form II implementation (2 state variables per biquad). The body formants are **fixed frequencies** independent of the string pitch — a guitar body resonates at the same frequencies regardless of which note is played (unlike a voice, which changes formants for different vowels). This is physically correct: the body is a passive resonator excited by the string.

**Presets** (body formants, string character, spectral centroid):
| Preset | F1–F4 (Hz) | Damping | Bright | Centroid | Character |
|--------|-------------|---------|--------|----------|-----------|
| Acoustic | 98/204/390/810 | 0.9985 | 0.50 | 2504 Hz | Steel-string dreadnought, balanced |
| Classical | 90/185/350/700 | 0.9975 | 0.25 | 2426 Hz | Nylon, warm and mellow |
| Banjo | 260/480/920/1800 | 0.9990 | 0.80 | 2906 Hz | Membrane body, bright twang |
| Sitar | 80/170/420/1100 | 0.9990 | 0.55 | 2294 Hz | Gourd + jawari buzz (0.6) |
| Oud | 75/155/310/620 | 0.9965 | 0.20 | 2211 Hz | Gut strings, darkest, shortest |
| Koto | 130/350/850/2200 | 0.9992 | 0.85 | 3184 Hz | Hard bridge, brightest, long ring |
| Harp | 100/250/500/1000 | 0.9995 | 0.60 | 2855 Hz | Minimal body, longest sustain |
| Ukulele | 180/380/720/1400 | 0.9970 | 0.35 | 2302 Hz | Small body, warm, percussive |

**Reference**:
- Elejabarrieta, Ezcurra & Santamaría, "Coupled modes of the resonance box of the guitar" (JASA, 2002) — measured guitar body mode frequencies and Q factors
- Fletcher & Rossing, "The Physics of Musical Instruments" (1998), Ch. 9 (guitars) — body mode analysis, soundboard coupling
- Karjalainen, Mäki-Patola & Kanerva, "Body Modeling Techniques for String Instrument Synthesis" (ICMC, 2004) — biquad body modeling approach
- Smith, "Physical Audio Signal Processing" (CCRMA) — pick position comb filter, string-body coupling

**Assessment**: Good coupled string+body model with strong per-preset differentiation. The per-preset damping (0.9965–0.9995) and brightness (0.20–0.85) create a 973 Hz spectral centroid spread between the darkest preset (oud, 2211 Hz) and brightest (koto, 3184 Hz) — a clearly audible difference. The amplitude-dependent delay modulation for jawari bridge buzz is physically correct (models parabolic bridge contact geometry) and produces the characteristic descending precursor sweep of sitar/tanpura. The DC blocker in the KS loop prevents the silence bug that occurred when stacking long-sustain harp notes. **Possible enhancements**: (1) String-body coupling feedback — body resonances modifying string vibration via bridge impedance. (2) Sympathetic string resonance — open strings ringing in response to played notes (defining sitar character). (3) Per-string body response for different bridge coupling angles.

### 1.21 Single/Double Reed (WAVE_REED)
**File**: `synth_oscillators.h`, init at `initReed()`, process at `processReedOscillator()`
**Algorithm**: Bore waveguide + pressure-driven reed reflection function (STK Clarinet-style).

**Physics model**: A reed (cane, plastic, or metal) acts as a pressure-controlled valve at the mouthpiece end of a bore waveguide. The bore delay line determines the pitch; the reed's nonlinear reflection function sustains oscillation by modulating the returning bore wave with mouth pressure.

**Signal flow**:
```
boreReturn = boreBuf[boreIdx]
lpState = LP_filter(boreReturn)         // bore wall losses
pMinus = -0.95 * lpState               // open-end reflection (invert + loss)
pressureDiff = pMinus - blowPressure
reedRefl = offset + slope * pressureDiff  // reed table, clamped [-1,1]
boreInput = Pm + pressureDiff * reedRefl
boreBuf[boreIdx] = boreInput
output = DC_block(boreReturn)
```

**Reed table**: Linear function with hard clamp — `offset + slope * x`, where offset (0.3–0.7) is the rest opening controlled by `aperture`, and slope (−0.4 to −0.9) is the stiffness response. Hard clamp to [−1,1] models the reed beating against the lay.

**Bore conicity**: The `bore` parameter (0=cylindrical, 1=conical) controls:
- LP filter coefficient (0.55 cylindrical → 0.92 conical) — cylindrical is darker
- Even harmonic injection for conical bores (asymmetric saturation via `tanh + x²` for bore > 0.3)
- This is the key timbral differentiator: clarinet (cylindrical, odd harmonics only) vs sax (conical, all harmonics)

**Parameters**: blowPressure (0–1), stiffness (0–1), aperture (0–1), bore (0–1), vibratoDepth (0–1).

**Presets** (6):
| # | Name | Bore | Character |
|---|------|------|-----------|
| 192 | Clarinet | 0.0 (cyl) | Dark, hollow, woody — odd harmonics |
| 193 | Soprano Sax | 0.9 (con) | Bright, cutting, edgy |
| 194 | Alto Sax | 0.75 (con) | Warm, round, classic jazz |
| 195 | Tenor Sax | 0.8 (con) | Rich, breathy, full |
| 196 | Oboe | 0.55 (con) | Nasal, penetrating — stiff double reed |
| 197 | Harmonica | 0.1 (cyl) | Warm, buzzy, blues — soft free reed |

**Reference**:
- McIntyre, Schumacher & Woodhouse, "On the Oscillations of Musical Instruments" (JASA 1983)
- Cook, "Real Sound Synthesis for Interactive Applications" (2002), Ch. 10
- Scavone, STK Clarinet class
- Guillemain et al., "Digital synthesis of self-sustained instruments" (2005)
- Bilbao, "Numerical Sound Synthesis" (2009) — reed valve discretization

**Assessment**: Good STK-derived model. The reed table (offset + slope, clipped) is the proven approach from Cook/Scavone. The bore conicity parameter successfully differentiates cylindrical (clarinet, odd harmonics) from conical (sax, all harmonics) via both the LP coefficient range and the even-harmonic injection nonlinearity. The 6 presets span a wide timbral range from hollow clarinet to nasal oboe to buzzy harmonica. **Possible enhancements**: (1) Register hole for overblowing (clarinet 12th, sax octave). (2) Tonehole scattering junction for more realistic fingering transitions. (3) Two-delay-line conical bore model (STK Saxofony) for more accurate sax physics.

### 1.22 Brass / Lip Valve (WAVE_BRASS)
**File**: `synth_oscillators.h`, init at `initBrass()`, process at `processBrassOscillator()`
**Algorithm**: Bore waveguide + memoryless lip nonlinearity (STK Brass-style).

**Physics model**: The player's lips act as a nonlinear reflector at the mouthpiece end of a bore waveguide. Unlike a physical lip oscillator (Vergez & Rodet 1997, Adachi & Sato 1995), STK uses a **memoryless nonlinearity** — the lip has no resonant frequency of its own, so the bore delay line alone determines the pitch. This approach always self-oscillates at the correct frequency.

**Signal flow**:
```
boreReturn = boreBuf[boreIdx]
lpState = LP_filter(boreReturn)         // bore wall + bell losses
pMinus = lpState * -0.95               // bell reflection
[optional: mute LP stage]
pressureDiff = pMinus - blowPressure
lipRefl = tanh((offset + slope * pressureDiff) * gain)  // lip table
boreInput = Pm + pressureDiff * lipRefl
boreBuf[boreIdx] = boreInput
output = DC_block(lpState)
```

**Lip nonlinearity**: Same structure as reed (offset + slope * input) but with `tanh()` soft-saturation instead of hard clamp. This produces smoother harmonic content — warmer and rounder than reed, matching the character of brass instruments where the lip mass smooths the valve action.

**Bore conicity**: `bore` parameter controls the LP filter coefficient:
- Cylindrical (trumpet, bore=0): LP coeff 0.90 → bright, projecting, strong upper harmonics
- Conical (French horn, bore=1): LP coeff 0.70 → dark, mellow, fewer overtones

**Lip tension**: Controls the `tanh` gain — higher tension → more aggressive saturation → brighter, brassier tone with more odd harmonics. Low tension → gentle curve → warm, round.

**Mute**: Optional second LP stage that darkens and nasalizes the tone (harmon/cup mute effect). Applied after the bell reflection, before the lip sees the returning wave.

**Parameters**: blowPressure (0–1), lipTension (0–1), lipAperture (0–1), bore (0–1), mute (0–1).

**Presets** (6):
| # | Name | Bore | Mute | Character |
|---|------|------|------|-----------|
| 204 | Trumpet | 0.0 | 0.0 | Bright, projecting, brassy |
| 205 | Muted Trumpet | 0.1 | 0.65 | Nasal, distant — Miles Davis |
| 206 | Trombone | 0.2 | 0.0 | Warm, powerful, slide character |
| 207 | French Horn | 0.85 | 0.0 | Dark, mellow, complex — conical |
| 208 | Tuba | 0.95 | 0.0 | Deep, round, very dark |
| 209 | Flugelhorn | 0.7 | 0.0 | Soft, lyrical, warm — between trumpet and horn |

**Reference**:
- Cook, "Real Sound Synthesis for Interactive Applications" (2002), Ch. 10
- Scavone, STK Brass class — memoryless lip nonlinearity approach
- Adachi & Sato, "Time-domain simulation of sound production in the brass instrument" (JASA 1995) — physical lip model (not used, but informed parameter choices)
- Vergez & Rodet, "Trumpet and trumpet player: model and simulation in a musical context" (ICMC 1997)
- Campbell, Gilbert & Myers, "The Science of Brass Instruments" (Springer 2021)

**Assessment**: Good STK-derived model. The memoryless lip approach is the correct engineering choice — it always oscillates at the bore pitch (no mode-locking issues), is CPU-cheap, and sounds musical. The `tanh` soft-saturation successfully differentiates brass from reed (which uses hard clamp). The single-LP + mute architecture keeps the feedback loop gain high enough for sustained oscillation while the bore parameter provides meaningful timbral variation from bright trumpet to dark tuba. The mute parameter adds a useful performance dimension. **Possible enhancements**: (1) Slide/valve transition effects — pitch glide between notes with characteristic "blat." (2) Growl/multiphonic — modulate lip tension with a low-frequency oscillator for vocal-fold interaction. (3) Bell radiation filter — frequency-dependent radiation pattern (highs are more directional in brass).

### 1.23 Shaker / Particle Collision Percussion (WAVE_SHAKER)
**File**: `synth_oscillators.h:3117–3300`
**Algorithm**: PhISM (Physically Informed Stochastic Model) — statistical particle collision with resonator bank
**Reference**: Cook, "Physically Informed Sonic Modeling (PhISM): Synthesis of Percussive Sounds" (Computer Music Journal, 1997). STK Shakers class.

**Architecture**: Unlike waveguide (reed/brass/pipe) or modal (mallet/membrane) engines, shakers use a stochastic excitation model. No delay lines, no mode summation — just random collision events filtered through resonators.

**Signal flow**:
```
Particle Energy (exponential decay per sample)
    │
    ├── Per-sample collision check (statistical, not per-particle)
    │   Expected collisions = numParticles × collisionProb × energy
    │   Each collision → random-amplitude impulse
    │
    └── Guiro mode: periodic collisions at scrapeRate Hz + jitter
            │
            v
    Resonator Bank (4 × SVF bandpass)
    Each resonator: Chamberlin SVF in bandpass mode
        fc = 2·sin(π·freq/sr), Q = freq/(bw+1)
        LP += fc·BP; HP = input − LP − BP/Q; BP += fc·HP
    Output = Σ(BP × gain) per resonator
            │
            v
    Output × soundLevel
```

**Key optimization**: No per-particle state. Cook's original PhISM uses statistical simulation — the number of collisions per sample follows a binomial distribution approximated by its expected value. At typical parameters this yields 0–2 collisions per sample, making the engine extremely cheap (~25 FLOPs/sample: 4 SVF iterations + collision logic).

**Collision model**: Each collision generates a noise impulse scaled by `collisionGain × particleEnergy × randomAmplitude(0.7–1.3)`. The random amplitude variation simulates particles hitting at different velocities. In guiro mode, collisions are periodic (phase accumulator at `scrapeRate` Hz) with configurable timing jitter.

**Resonator bank**: 4 Chamberlin SVF bandpass filters, each with preset-specific center frequency, bandwidth, and gain. These model the container's acoustic resonances — a gourd (maraca) has different modes than metal jingles (tambourine) or bamboo tubes. The SVF topology is identical to the formant filters in WAVE_VOICE (§1.4).

**Energy model**: `particleEnergy` starts at 1.0 on note-on and decays by `systemDecay` each sample (e.g., 0.9994 = ~6600 samples ≈ 150ms at 44.1kHz). While the ADSR envelope is in attack/decay/sustain (key held), energy is floored at 0.3 — continuous shaking. On retrigger (envelope restart), energy re-boosts to 1.0 for a fresh shake burst.

**Parameters**: particles (0–1 → 8–64 count), decayRate (0–1 → systemDecay 0.998–0.9999), resonance (bandwidth scaling 1.5x–0.2x), brightness (frequency shift 1.0x–1.8x), scrape (0=random, 1=fully periodic at 200 Hz).

**Presets** (8):
| # | Name | Particles | Decay | Resonators | Character |
|---|------|-----------|-------|------------|-----------|
| 226 | Maraca | 20 | 0.9994 | 3.2/6.5/9.8/1.8 kHz | Classic Latin shaker — gourd shell |
| 227 | Cabasa | 40 | 0.9990 | 5.0/8.2/11.5/3.5 kHz | Metal beads on gourd — bright, snappy |
| 228 | Tambourine | 24 | 0.9993 | 2.8/5.6/8.4/11.2 kHz | Tonal jingles — tight Q, sustains |
| 229 | Sleigh Bells | 48 | 0.9997 | 4.5/6.8/9.2/12.0 kHz | Bright metal, many particles, long shimmer |
| 230 | Bamboo | 10 | 0.9996 | 1.2/2.8/4.2/0.8 kHz | Sparse woody chime — lower, fewer hits |
| 231 | Rain Stick | 64 | 0.9998 | 1.6/4.0/7.5/2.4 kHz | Ambient cascade — wide BW, very slow decay |
| 232 | Guiro | 12 | 0.9992 | 2.5/4.0/5.8/1.5 kHz | Periodic scraping at 120 Hz + jitter |
| 233 | Sandpaper | 50 | 0.9985 | 4.0/8.0/2.0/12.0 kHz | Dense noise burst — wide BW, short |

**Assessment**: Faithful implementation of Cook's PhISM. The statistical collision approximation is the correct choice — it's what the original paper recommends, and avoids the O(N) per-particle overhead that would make 64-particle presets expensive. The SVF resonator bank gives good timbral variety across presets. The sustained-energy floor and retrigger re-boost are extensions beyond the original PhISM that make the engine more playable as a musical instrument (not just a one-shot trigger). **Possible enhancements**: (1) Velocity-sensitive particle count (harder hits = more particles). (2) Position modulation — tilt the shaker over time to shift resonator frequencies. (3) Per-resonator decay (different modes ring out at different rates, like metallic). (4) Sympathetic excitation from other voices (shaker rattles when bass hits).

### 1.24 Stiff String / Piano (WAVE_STIFKARP)
**File**: `synth_oscillators.h`, init at `initStifKarpExcitation()`, `initStifKarpDispersion()`, `initStifKarpPreset()`, process at `processStifKarpOscillator()`
**Algorithm**: Karplus-Strong (reusing WAVE_PLUCK) + cascaded allpass dispersion chain for inharmonicity + per-preset output tone filter + 4 biquad body resonators.
**Reference**: Jaffe & Smith, "Extensions of the Karplus-Strong Plucked-String Algorithm" (CMJ, 1983). Bank, "Physics-Based Sound Synthesis of the Piano" (Helsinki Univ. Tech., 2000). STK StifKarp class.

**Signal flow**:
```
Noise burst ──→ hammer hardness LP ──→ normalize ──→ pick position comb
                                                           │
KS delay line ◄────────── Thiran fractional delay ◄── allpass dispersion
     │                                                      ▲
     └──→ KS loop filter (avg + brightness + damping) ──────┘
                                                    │
     stringSample ──→ output tone filter (1-pole LP, per-preset)
                           │
                      ├──→ buzz (prepared piano, tanh soft-clip)
                      │
                      ├──→ 4× biquad BPF body ──→ crossfade mix ──→ output
                      │
                      └──→ sympathetic feedback (3rd partial tap)
```

**Key differentiator — allpass dispersion chain**: Real stiff strings (piano wire, metal plates) exhibit frequency-dependent phase velocity — higher harmonics arrive slightly sharp of integer ratios. This **inharmonicity** is characterized by `f_n = n × f₁ × √(1 + B × n²)`, where B ranges from ~0.0001 (bass piano strings) to ~0.01 (treble).

A cascade of 1-4 first-order allpass filters in the delay loop approximates this dispersion:
```
for each stage i (1-4):
    C[i] = (1 − phaseTarget) / (1 + phaseTarget)
    apOut = C[i] × input + state[i]
    state[i] = input − C[i] × apOut
    input = apOut
```
Where `B = stiffness² × 0.015 × freqScale` and `phaseTarget = B × (i+1) × freq / sr`. The number of active stages scales with the stiffness parameter (low stiffness = 1 stage, high = 4).

**Hammer hardness shaping**: Initial noise burst LP-filtered at note-on. `hardness` 0 (soft felt) → heavy filtering leaving fundamental-heavy energy. `hardness` 1 (hard steel) → full noise spectrum. Normalized post-filter to prevent volume differences.

**Output tone filter**: 1-pole LP on the output (NOT in the feedback loop — avoids killing sustain). Per-preset `loopFilterCoeff` controls character with a 7× range:
- 0.10 = clavichord (very dark, muffled)
- 0.18 = grand piano (warm felt)
- 0.70 = harpsichord (bright, jangly)

This is the primary timbral differentiator between presets.

**Damper**: Modulates effective per-sample damping: `effectiveDamping = ksDamping × (0.992 + 0.008 × damperState)`. At full damper (1.0) = normal sustain. At zero = 0.8% additional per-sample loss. Smooth ramp (0.0005/sample) prevents clicks.

**Body resonator**: 4 parallel biquad BPFs (same architecture as WAVE_GUITAR). Mix uses proper crossfade: `out × (1 − bodyMix×0.6) + bodyOut × bodyMix`.

**Sympathetic resonance**: Output fed back into delay line at `ksLength/3` tap point at low level (0.015 × sympatheticLevel), exciting the 3rd partial.

**Presets** (8):
| # | Name | Tone | Body | F1-F4 (Hz) | Character |
|---|------|------|------|-------------|-----------|
| 218 | Grand Piano | 0.18 | 0.70 | 65/130/260/520 | Warm, spruce soundboard |
| 219 | Bright Piano | 0.30 | 0.65 | 60/125/280/600 | Concert grand, shimmer |
| 220 | Harpsichord | 0.70 | 0.25 | 200/400/900/1800 | Bright plectrum snap |
| 221 | Dulcimer | 0.50 | 0.75 | 180/360/720/1500 | Metallic wire shimmer |
| 222 | Clavichord | 0.10 | 0.10 | 150/300/600/1200 | Very dark, intimate |
| 223 | Prepared | 0.15 | 0.60 | 65/140/280/550 | Dark + bolt buzz (0.4) |
| 224 | Honky Tonk | 0.20 | 0.35 | 70/145/300/600 | Muffled, unison beating |
| 225 | Celesta | 0.60 | 0.50 | 400/800/1600/3200 | Metal plates, bell-like |

**Assessment**: Good stiff-string model filling the "metal string" gap between WAVE_PLUCK (nylon/gut) and WAVE_MALLET (bars). The allpass dispersion chain is the standard approach and successfully stretches upper partials for piano shimmer. The per-preset output tone filter (0.1-0.7 range) provides strong timbral differentiation — spectral centroids span 917-2170 Hz across presets. Memory cost is ~190 bytes/voice (reuses existing `ksBuffer[2048]`). **Possible enhancements**: (1) Velocity-dependent hammer hardness — harder at ff, softer at pp. (2) Multi-string coupling — 2-3 strings per note with slow beating. (3) Global sustain pedal — prevent note-off from engaging damper. (4) Duplex scale resonance.

### 1.25 Banded Waveguide (WAVE_BANDEDWG)
**File**: `synth_oscillators.h:3327–3484`
**Algorithm**: Banded waveguide synthesis — N parallel delay lines with tight BiQuad bandpass resonators (not KS lowpass), driven by bow friction or strike excitation. Matched to STK BandedWG.
**Reference**: Essl & Cook, "Banded Waveguides on Circular Topologies and of Beating Modes: Tibetan Singing Bowls and Glass Harmonicas" (ICMC 1999). STK BandedWG class.

**Architecture**: Unlike standard waveguides (reed/brass — single bore delay line) or modal synthesis (mallet/membrane — sinusoidal modes), BandedWG places each vibration mode in its own feedback delay line. Modes are self-sustaining under continuous excitation (bowing), producing the characteristic sustained tones of glass and metal instruments.

**Signal flow** (matched to STK BandedWG tick loop):
```
1. Compute excitation (same input for ALL modes):
   Bow mode:  velocitySum = Σ(gain[k] × delay[k])
              deltaV = bowVelocity - velocitySum
              input = deltaV × bowTable(deltaV) / nModes
   Strike:    input = strikeEnergy × noise × 0.1

2. For each mode m (4 parallel):
   bpInput = input + feedbackGain[m] × delay[m]
   bpOut = BiQuad_bandpass(bpInput)    ← tight 2-pole resonator (R≈0.9977)
   delay[m].write(bpOut)               ← clamp ±1.0
   out += bpOut

3. out → DC blocker → × soundLevel → Output
```

**Critical design**: The BiQuad bandpass (not KS lowpass) is what makes glass sound glassy. KS lowpass progressively dulls all harmonics (string timbre). The tight bandpass keeps each mode ringing at its exact frequency with no spectral evolution — pure partials that only change in amplitude, which is how glass and metal behave in reality.

**Memory**: Zero additional cost — partitions `ksBuffer[2048]` (already in every Voice struct for pluck/guitar/stifkarp, mutually exclusive). At C4 (261.6 Hz) with glass ratios (1.0, 2.32, 4.25, 6.63): delay lengths are 168+76+42+27 = 313 samples. Even at A1 (55 Hz): ~1334 total, well within 2048.

**BiQuad bandpass resonator**: 2-pole all-pole resonator per mode. Radius R = 1 − π·32/sampleRate ≈ 0.9977 at 44.1kHz (from STK). Coefficients: a1 = −2R·cos(θ), a2 = R², b0 = 1−R² (normalized for unity peak gain). This creates an extremely narrow resonance peak — each mode only allows energy at its exact frequency to circulate.

**Bow excitation** (STK BowTable): `f(x) = (|x·3 + 0.001| + 0.75)^(−4)`, clamped [0.01, 0.98]. This is a bell-shaped friction curve — maximum friction when bow-string velocity difference is small (sticking), drops off when slipping. Sum of all delay outputs weighted by feedbackGain is subtracted from bowVelocity, then multiplied by the bow table output. Same input is fed to ALL modes equally (divided by nModes).

**Strike excitation**: At note-on, delay lines are pre-loaded with noise proportional to delay length (STK pluck pattern: longer delay lines get more samples filled). Residual energy decays at 0.9995/sample (~3s to -60dB).

**Mode frequency ratios** (physics-derived, per preset):
| Preset | Ratios | Physical basis |
|--------|--------|----------------|
| Glass | 1.0, 2.32, 4.25, 6.63 | Flexural modes of a cylinder (STK preset 3) |
| Singing Bowl | 1.0, 1.002, 2.98, 2.99 | Near-degenerate pairs for beating (STK preset 4) |
| Vibraphone | 1.0, 2.756, 5.404, 8.933 | Free-free uniform bar (STK preset 0) |
| Wine Glass | 1.0, 2.32, 4.25, 6.63 | Thin shell (same as glass) |
| Prayer Bowl | 1.0, 2.71, 5.13, 8.27 | Bowl modes, wider spacing than singing bowl |
| Tubular | 1.0, 4.0198, 10.7184, 18.0697 | Tuned bar (STK preset 1) — wide spacing |

**Parameters**: bowPressure (0–1: friction force), bowSpeed (0–1: bow velocity), strikePos (0–1: excitation position — `sin²` weighting), brightness (0–1: maps to reflCoeff 0.85–0.995), sustain (0–1: scales bandpass bandwidth 1.5×–0.2×).

**Presets** (6):
| # | Name | Excitation | Ratios | Character |
|---|------|-----------|--------|-----------|
| 234 | Glass Harmonica | bow | glass | Pure, ethereal, slow onset (attack 150ms) |
| 235 | Singing Bowl | bow | bowl | Warm, beating between close modes, meditative |
| 236 | Bowed Vibes | bow | bar | Bright metallic sustain |
| 237 | Wine Glass | bow | glass | Very pure, high, delicate (attack 250ms) |
| 238 | Prayer Bowl | strike | bowl | Deep, long ring, one-shot |
| 239 | Tubular Chime | strike | tube | Bright, harmonic, one-shot |

**Assessment**: Faithful implementation of Essl & Cook / STK BandedWG. The tight BiQuad bandpass resonators (R≈0.9977) are the key difference from KS — they keep each mode ringing as a pure partial with no spectral evolution, which is what makes glass and metal instruments sound crystalline rather than stringy. The STK bow table (bell-shaped friction curve with ⁻⁴ exponent) creates stable self-sustaining oscillation. Singing bowl uses near-degenerate mode pairs (1.0/1.002) for characteristic beating. Tubular chime uses STK's tuned bar ratios (1.0, 4.02, 10.72, 18.07) for wide spacing — sonically very different from prayer bowl. Memory cost is zero (reuses ksBuffer[2048]). **Possible enhancements**: (1) Expand singing bowl to 12 modes (STK uses 12 for full pair structure). (2) Integration constant for bow velocity smoothing (STK CC#11). (3) Per-mode excitation weighting for struck presets (STK Tibetan bowl has non-uniform excitation). (4) Sympathetic mode excitation across voices.

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

Signal flow: **Octaver → Tremolo → Wah → Distortion → Bitcrusher → Chorus → Flanger → Phaser → Comb → Ring Mod → Tape → Vinyl → Delay → [Dub Loop] → Rewind → TapeStop → BeatRepeat → DJFX Loop → Reverb → HalfSpeed**

### 5.1 Octaver (Sub-Octave Generator)
**File**: `effects.h` (processOctaver)
**Algorithm**: Zero-crossing detection → flip-flop square wave at half frequency → multiply with input → lowpass tone control.
**Core logic**:
```
if (sample crosses zero): flipFlop = −flipFlop
sub = sample × flipFlop × subLevel
filterLp += tone² × 0.5 × (sub − filterLp)    // 1-pole LP smoothing
output = dry × (1 − mix) + (dry + sub) × mix
```
**Parameters**: Mix (0–1), SubLevel (0–1), Tone (0–1 → lowpass cutoff on sub signal).
**Chain placement**: First in chain (before tremolo). Needs the cleanest possible signal for reliable zero-crossing tracking.
**Reference**: Classic analog octaver design (Boss OC-2, EHX Octave Multiplexer). The zero-crossing → flip-flop → multiply topology is exactly how the OC-2's CMOS divider works: a Schmitt trigger detects zero crossings, a T flip-flop divides the frequency by 2, and the flip-flop output gates the original signal. The lowpass tone control models the OC-2's output filter that smooths the sub into a rounder shape.
**Assessment**: Authentic analog octaver behavior. The zero-crossing approach works best on clean, monophonic signals — on polyphonic material or noisy signals it will produce a buzzy, glitchy sub-octave, which is actually the correct behavior of real analog octavers (the OC-2 is famously finicky about input signal quality). The 1-pole lowpass tone control is simple but effective — it smooths the harsh square-wave edges of the sub signal into a warmer, more usable tone. Real OC-2 circuits use a 2-pole LP for the sub output, so the tone control here is slightly gentler (6 dB/oct vs 12 dB/oct), but adequate for the musical application. **Possible enhancement**: Octave-up mode via full-wave rectification (`|sample|`), which doubles frequency rather than halving it — classic Octavia/Green Ringer sound.

### 5.2 Tremolo (Volume LFO)
**File**: `effects.h` (processTremolo)
**Algorithm**: LFO-modulated volume attenuation with 3 waveform shapes.
**Core logic**:
```
switch (shape):
  SINE:     mod = 0.5 + 0.5 × sin(phase × 2π)
  SQUARE:   mod = (phase < 0.5) ? 1.0 : 0.0
  TRIANGLE: mod = phase < 0.5 ? phase×4−1 : 3−phase×4; mod = mod×0.5+0.5
output = sample × (1 − depth × (1 − mod))
```
**Parameters**: Rate (0.5–20 Hz), Depth (0–1), Shape (sine/square/triangle).
**Amplitude formula**: At depth=1, the signal swings between full volume (mod=1) and silence (mod=0). At depth=0.5, it swings between full and 50%. This is the standard "unipolar modulation depth" convention used by Fender amps, Boss TR-2, and most tremolo implementations.
**Reference**: The simplest and oldest modulation effect — built into Fender amps since 1955 (Tremolux). The amplitude modulation formula `1 − depth × (1 − mod)` follows the convention where depth=0 means no effect and depth=1 means full modulation down to silence. This is the "bias-varying" tremolo (amplitude modulation) as distinct from Leslie-style tremolo (which is actually frequency modulation via Doppler shift). Equivalent to Boss TR-2, EHX Pulsar, Fender Vibrato channel.
**Assessment**: Textbook correct. The three waveform shapes cover all classic tremolo styles: sine (smooth Fender-style), square (hard "chop" used in Morse code and stutter effects), triangle (gentle linear pulse). The LFO runs at audio-rate precision (per-sample phase increment), so even at 20 Hz there's no stepping or zipper noise. No tempo sync (could be added for rhythmic tremolo, but free-running Hz is the traditional approach). **Note**: The square shape creates an abrupt volume gate with no smoothing at the transitions, which can produce clicks at very low rates — real hardware often has a slight slew on the square wave transitions. For this application the raw square is fine (it's the desired "helicopter chop" effect).

### 5.3 Wah / Auto-Wah (Swept Bandpass Filter)
**File**: `effects.h` (processWah)
**Algorithm**: State Variable Filter (Cytomic/Simper SVF) in bandpass mode, with center frequency controlled by LFO or envelope follower.
**SVF (bandpass core)**:
```
g = tan(π × freq / sr)
k = 2 − 2 × resonance × 0.99
a1 = 1 / (1 + g(g + k))
a2 = g × a1,  a3 = g × a2
v3 = input − ic2eq
v1 = a1 × ic1eq + a2 × v3       // bandpass output
v2 = ic2eq + a2 × ic1eq + a3 × v3
ic1eq = 2v1 − ic1eq;  ic2eq = 2v2 − ic2eq
```
**Sweep modes**:
- **LFO** (WAH_MODE_LFO): Sine LFO at `wahRate` Hz → sweep position (0–1). Classic auto-wah rhythm.
- **Envelope** (WAH_MODE_ENVELOPE): Peak envelope follower with fast attack (~0.01), slow release (~0.0001). Sensitivity scales input level. Harder playing = higher sweep = brighter.

**Frequency mapping**: Exponential sweep from `freqLow` to `freqHigh`: `freq = freqLow × (freqHigh/freqLow)^sweep`. The exponential mapping is essential — human pitch perception is logarithmic, so a linear frequency sweep would sound like it's spending too long in the high range.
**State clamping**: SVF states clamped to ±4 to prevent blowup at high resonance.
**Parameters**: Mode (LFO/Envelope), Rate (0.5–10 Hz), Sensitivity (0.1–5.0), FreqLow (200–800 Hz), FreqHigh (800–4000 Hz), Resonance (0–1), Mix (0–1).
**Reference**: Classic wah-wah pedal (Cry Baby, Dunlop GCB-95) is a single bandpass filter with foot-controlled center frequency. Auto-wah (Mutron III, Boss AW-3) replaces the foot controller with an envelope follower. The SVF bandpass is the modern digital equivalent of the original inductor-capacitor resonant circuit in the Cry Baby. The exponential frequency mapping matches how real wah pots work (log-taper potentiometer sweeps frequency exponentially).
**Assessment**: Good quality. The Cytomic SVF gives correct bandpass behavior without the frequency warping issues of the Chamberlin SVF (which the bus filter section also uses). The exponential frequency sweep is the correct mapping for musical results. The envelope follower is a simple peak detector — real Mutron III uses an RMS detector with a more complex time constant, but the peak detector is adequate and snappier. The resonance range (up to 0.99) allows near-self-oscillation for quacky wah sounds without going unstable. **Possible enhancements**: (1) A "manual" mode where the sweep position is a static knob (for manual wah control). (2) An "expression" mode for MIDI CC control. (3) Band-boosted LP mode alongside the BP, as some wah pedals (like the original Cry Baby) use a resonant lowpass rather than pure bandpass.

### 5.4 Ring Modulator (as Effect)
**File**: `effects.h` (processRingMod)
**Algorithm**: Multiply signal by a carrier sine oscillator at a fixed frequency.
**Core logic**:
```
carrier = sin(phase × 2π)
phase += freq / sampleRate
wet = sample × carrier
output = dry × (1 − mix) + wet × mix
```
**Parameters**: Freq (20–2000 Hz), Mix (0–1).
**Reference**: Ring modulation produces sum and difference frequencies: if the input has a component at frequency `f`, and the carrier is at `fc`, the output contains `f+fc` and `f−fc` (but NOT the original `f` or `fc`). Named after the ring of diodes used in the original analog circuit (patented 1934, used in telephone multiplexing). Classic audio effects: Moog MF-102 Ring Modulator, EHX Ring Thing, Bode Frequency Shifter (which is a single-sideband ring modulator). Also the basis of the Dalek voice effect.
**Note on voice-level vs effect-level**: PixelSynth already has ring modulation on the synth voice (`p_ringMod` in synth.h, which modulates at a multiple of the voice's own base frequency). This effect-level ring mod is different — it uses a fixed carrier frequency independent of the input pitch, which produces the characteristic inharmonic, metallic, bell-like tones. The voice-level ring mod tracks pitch (producing related harmonics); the effect-level one doesn't (producing atonal sum/difference frequencies).
**Assessment**: Textbook correct. The implementation is the simplest possible ring modulator — a single carrier × signal multiplication with dry/wet blend. This is exactly what the Moog MF-102 does internally (minus the LFO modulation of the carrier frequency, which could be added). The fixed-frequency carrier produces the classic "robot voice" / metallic bell character. **Possible enhancements**: (1) LFO on the carrier frequency for time-varying metallic textures. (2) Square/triangle carrier options (different harmonic content in the sidebands). (3) Feedback path (carrier × signal → feed back into input) for more aggressive distortion character.

### 5.5 Distortion
**File**: `effects.h` (processDistortion)
**Algorithm**: Same waveshaping modes as voice distortion (section 4.1) + post-distortion 1-pole LP tone control.
**Tone**: `cutoff = tone²·0.5 + 0.1` — exponential curve, darker at low values.
**Assessment**: Adequate. The tone control after distortion is essential for taming harshness. Same assessment as 4.1.

### 5.6 Bitcrusher
**File**: `effects.h` (processBitcrusher)
**Algorithm**: Sample rate reduction + bit depth reduction.
**Rate reduction**: Hold sample value for `crushRate` consecutive samples (zero-order hold).
**Bit reduction**: `floor(sample × levels) / levels` where `levels = 2^bits`.
**Reference**: Standard bitcrusher algorithm. The zero-order hold is the simplest approach (more sophisticated approaches would use decimation + interpolation).
**Assessment**: Correct. The zero-order hold creates the characteristic staircase aliasing. The bit reduction correctly quantizes to N-bit resolution. No dithering is applied (which is intentional — the quantization noise IS the effect).

### 5.7 Chorus (Classic + BBD Mode)
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

### 5.8 Flanger
**File**: `effects.h` (processFlanger)
**Algorithm**: Short modulated delay with feedback, triangle LFO.
**Delay range**: 0.1–10 ms. Buffer: 512 samples (~11 ms).
**LFO**: Triangle wave (smoother than sine for flanging sweeps).
**Feedback**: ±0.95 (negative inverts phase → hollow "through-zero" character).
**Soft clipping**: Feedback clamped to ±1.5.
**Reference**: Standard flanger design. The through-zero flanging with negative feedback is the classic technique.
**Assessment**: Correct. The triangle LFO is the standard choice for flangers (smoother sweep than sine). Feedback clamping prevents runaway. Short delay times and high feedback create the characteristic jet-engine sound.

### 5.9 Phaser
**File**: `effects.h` (processPhaser)
**Algorithm**: Cascade of 2–8 first-order allpass filters, LFO modulates allpass coefficient.
**Allpass**: `output = −coeff × input + prev; prev = coeff × output + input` (standard first-order allpass).
**LFO**: Sine wave modulates coefficient from 0.1 to 0.9.
**Feedback**: Output → input (0 to 0.9).
**Reference**: Standard phaser design. See Smith, "Physical Audio Signal Processing", chapter on allpass filters.
**Assessment**: Correct. Each allpass stage creates one notch in the frequency response. 4 stages (classic) = 2 notches, 8 stages = 4 notches. The sine LFO sweeps the notch frequencies for the characteristic sweeping effect. The allpass implementation is the standard textbook form.

### 5.10 Comb Filter
**File**: `effects.h` (processComb)
**Algorithm**: Feedback delay line tuned to a specific pitch, with damping LP in feedback path.
**Delay**: `samples = sr / frequency` (20–2000 Hz range).
**Feedback**: −0.95 to +0.95 (negative = hollow/nasal).
**Damping**: 1-pole LP on feedback (frequency-dependent loss per iteration).
**Reference**: Standard comb filter. This is essentially the same topology used in Karplus-Strong synthesis and in the reverb's comb filters.
**Assessment**: Correct. The damping LP in the feedback path prevents infinite ringing at high frequencies, which makes the resonance more natural-sounding. Negative feedback creates the "hollow" character by emphasizing odd harmonics.

### 5.11 Tape Simulation
**File**: `effects.h` (processTape, tapeWowFlutterLFO)
**Algorithm**: Saturation + noise-modulated wow/flutter + hiss.
**Saturation**: `tanh()` soft clipping.
**Wow/Flutter**: Noise-modulated sine LFOs via `tapeWowFlutterLFO()`:
- Core sine provides fundamental periodicity (real capstan rotation IS roughly periodic)
- LP-filtered white noise (per-LFO PRNG + 1-pole LP at ~3–4× the LFO rate) adds cycle-to-cycle irregularity
- Blend: `sine × (1 − noiseAmt) + (sine + filteredNoise) × 0.5 × noiseAmt × 2`
- Wow: 0.5 Hz, ±200 samples (±4.5 ms), 35% noise blend, LP cutoff ~1.5 Hz. Models capstan eccentricity and supply reel tension drift.
- Flutter: 6 Hz, ±40 samples (±0.9 ms), 40% noise blend (more erratic), LP cutoff ~24 Hz. Models motor jitter and bearing wobble.
**Hiss**: LP-filtered white noise (3 kHz corner).
**Buffer**: 4096 samples (~93 ms) circular delay.
**Reference**: Standard tape simulation approach. Wow/flutter rates and depths match typical cassette player measurements (IEC 386). Noise-modulated LFO technique from Välimäki & Bilbao, "Virtual Analog Effects" (2010). The noise blend ratios (35%/40%) are calibrated to match measurements of consumer cassette decks where wow is dominated by capstan period (more periodic) and flutter by motor/bearing irregularities (more random).
**Assessment**: Good. The noise modulation gives each wow/flutter cycle a unique shape while preserving the fundamental periodicity that makes it sound mechanical rather than random. The LP filtering on the noise component is essential — without it, the noise adds audible pitch jitter rather than slow shape variation. The saturation is basic tanh (real tape has frequency-dependent saturation, but for a game audio context this is adequate).

### 5.12 Vinyl Simulation
**File**: `effects.h` (processVinylSim)
**Algorithm**: Pitch warp + surface noise + crackle + 2-pole tone LP.
**Warp**: Slow sine LFO modulates delay read position (±150 samples = ±3.4 ms).
**Noise**: LP-filtered white noise (3 kHz corner).
**Crackle**: Sparse pops (random noise > threshold, sparse gating).
**Tone**: Cascaded 2× 1-pole LP (800 Hz–16 kHz range).
**Reference**: Common vinyl simulation technique. See Esquef & Välimäki, "Interpolation of Long Gaps in Audio Signals" for vinyl noise modeling.
**Assessment**: Decent for character. The crackle model is simplistic (real vinyl crackle has a specific transient shape and spectral content related to physical groove damage). The 2-pole tone filter correctly models the HF rolloff of vinyl playback (RIAA-ish). The pitch warp captures the basic "warped record" effect.

### 5.13 Delay
**File**: `effects.h` (processDelay)
**Algorithm**: Circular buffer with Hermite interpolation, filtered feedback.
**Buffer**: 2× sr samples (~2 seconds at 44.1 kHz).
**Interpolation**: Cubic Hermite for smooth delay time modulation.
**Feedback filter**: 1-pole LP with exponential tone control (`tone²·0.4 + 0.1`).
**Reference**: Standard digital delay design.
**Assessment**: Good. Hermite interpolation prevents clicks when delay time is modulated. The tone filter on feedback creates progressively darker repeats, which is the standard behavior of tape and analog delays.

### 5.14 Send Delay
**File**: `effects.h` (_processDelaySendCore)
**Algorithm**: Identical to master delay but in a separate buffer, always running.
**Use**: Shared delay effect fed by per-bus delaySend knobs (send/return architecture).
**Assessment**: Correct send/return topology.

### 5.15 Leslie Rotary Speaker
**File**: `effects.h` (processLeslie), chain position: after Wah, before Distortion
**Algorithm**: Crossover split → independent horn (treble) and drum (bass) rotor simulation with AM + Doppler pitch modulation, tube preamp overdrive, and spin-up/down inertia modeling.

**Physics model**: The Leslie 122 (1941) is a rotating speaker cabinet with two elements: a treble horn (directional, spinning fast) and a bass drum (omnidirectional baffle, spinning slow). The rotation creates amplitude modulation (directional beam pattern) and Doppler pitch shift (changing distance to microphone). The horn and drum have different masses, so they accelerate and decelerate at different rates — the mismatch creates the signature "swirl" during speed transitions.

**Signal flow**:
1. **Pre-amp overdrive**: Padé approximant of tanh — `x(27+x²)/(27+9x²)`, gain 1–6×. Models the tube preamp that drives the Leslie amp. Even-harmonic character from the asymmetric tube response.
2. **1-pole crossover** at 800 Hz: `LP += 0.1074 × (input − LP)`, `HP = input − LP`. Perfect reconstruction (LP + HP = original). Coefficient precomputed: `1 − exp(−2π×800/44100) ≈ 0.1074`. Authentic — the real Leslie 122 uses a simple passive LC crossover.
3. **Drum rotor** (bass, <800 Hz): Sine AM at 30% depth. `drumAM = 1 − 0.3 × (0.5 − 0.5 × cos(drumPhase × 2π))`. Gentle modulation — bass frequencies are felt, not heard spinning.
4. **Horn rotor** (treble, >800 Hz): Shaped AM + Doppler via modulated delay.
   - **AM**: `0.5 + 0.5×cos(θ) + 0.12×cos(2θ)`, scaled to 0.1–1.0. The 2nd harmonic creates a sharper peak (horn facing mic) and flatter trough (horn facing away), modeling the directional beam pattern of the real horn bell.
   - **Doppler**: 512-sample circular delay buffer. Base delay 3.0ms (horn-to-mic distance ~1m), ±0.5ms excursion (horn radius ~15cm). Read position modulated by `sin(hornPhase × 2π) × dopplerDepth`. Hermite interpolation for smooth pitch transitions.
5. **Recombine**: Balance control (0=drum only, 0.5=equal, 1=horn only), dry/wet mix.

**Rate slewing** (the key to the Leslie sound):
| Parameter | Value | Source |
|---|---|---|
| Horn slow (chorale) | 0.8 Hz (48 RPM) | Smith/Abel 1999 |
| Horn fast (tremolo) | 6.6 Hz (396 RPM) | Smith/Abel 1999 |
| Drum slow | 0.7 Hz (42 RPM) | Smith/Abel 1999 |
| Drum fast | 5.8 Hz (348 RPM) | Smith/Abel 1999 |
| Horn accel TC | 0.7s | Light plastic horn |
| Horn decel TC | 1.2s | Coasts longer |
| Drum accel TC | 4.0s | Heavy wooden drum |
| Drum decel TC | 5.0s | Heavy inertia |

Exponential slew: `rate += (target − rate) × dt/TC`. The horn reaches full speed in ~0.7s while the drum takes ~4s — during a slow→fast transition, the horn is already at full tremolo while the drum is still lumbering up. This mismatch creates the classic Leslie "swirl" that no simple tremolo can replicate.

**Speed modes**: Stop (0 Hz), Slow/Chorale, Fast/Tremolo. Matches the real Leslie's 2-position rocker switch + brake.

**Parameters**: leslieSpeed (3-way: stop/slow/fast), leslieDrive (0–1), leslieBalance (0–1), leslieDoppler (0–1), leslieMix (0–1).

**Bus version**: Full per-bus Leslie available (independent state per bus), enabling dedicated organ bus routing.

**Reference**:
- Smith & Abel, "Closed-Form Swept Sine Delay for Leslie Speaker" (ICMC 1999) — Doppler delay analysis, rotation speed measurements
- Werner & Smith, "The Leslie Speaker Cabinet" (DAFx 2016) — comprehensive cabinet model
- Pekonen et al., "Computationally Efficient Hammond Organ Synthesis" (DAFx 2011) — Leslie parameters

**Assessment**: Good mono Leslie model. The dual-rotor simulation with independent slew rates correctly captures the most important perceptual feature — the speed transition "swirl." The shaped horn AM (cos + 2nd harmonic) is more accurate than a simple sine and produces the characteristic asymmetric amplitude pattern of a directional horn. The Doppler delay with Hermite interpolation produces clean pitch modulation without aliasing. **Possible enhancements**: (1) Stereo output — real Leslie recordings use two mics, producing L/R phase spread that's impossible to replicate in mono. (2) Cabinet resonance/coloring — the wooden cabinet has its own frequency response. (3) Horn reflection — at close mic distance, the horn produces a comb-filter effect from the reflected path off the cabinet walls.

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
**Wow**: 0.4 Hz noise-modulated sine, ±40 samples, 35% noise blend, LP cutoff ~1.2 Hz.
**Flutter**: 5 Hz noise-modulated sine, ±8 samples, 40% noise blend, LP cutoff ~20 Hz.
Combined offset applied to read position. Uses same `tapeWowFlutterLFO()` helper as the tape effect.
**Assessment**: Good. The noise modulation makes the dub loop's tape character more organic — especially noticeable on long feedback tails where each echo's pitch wobble is slightly different.

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
**Per-bus chain**: Octaver → Tremolo → Wah → Filter (SVF) → EQ → Distortion → Compressor → Chorus (Classic or BBD) → Phaser → Comb → Ring Mod → Delay → Reverb Send → Delay Send.
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
| Electric Piano | 12-mode modal + register-scaled pickup nonlinearity (3 types) | Good | Shear 2011, Gabrielli 2020, real sample comparison |
| Organ (Hammond) | 9-drawbar sine bank + click/perc/scanner V/C | Good | Pekonen 2011, Pakarinen 2009 |
| Metallic Perc | 6-osc ring-mod pairs + per-partial decay | Good | TR-808/909 service manuals |
| Guitar Body | KS string + pick comb + 4 biquad body modes | Good | Elejabarrieta 2002, Karjalainen 2004 |
| Stiff String | KS + allpass dispersion + tone filter + body | Good | Jaffe & Smith 1983, Bank 2000, STK StifKarp |
| Shaker (PhISM) | Statistical collision + 4× SVF resonators | Good | Cook 1997, STK Shakers |
| Leslie Speaker | Crossover + dual-rotor AM/Doppler + slew | Good | Smith & Abel 1999, Werner 2016 |
| SVF Filter | Simper/Cytomic TPT | Excellent | Simper 2013, Zavalishin 2012 |
| Ladder Filter | 4-pole TPT + 2× OS, true multimode | Excellent | KR-106, Zavalishin 2012 |
| One-pole Filter | 1st-order IIR | Adequate | Textbook |
| Reso Bandpass | Chamberlin feedback | Adequate | Chamberlin 1985 |
| Reverb (Schroeder) | 4 combs + 2 allpass | Adequate | Schroeder 1962, Moorer 1979 |
| Reverb (FDN) | 8-line Hadamard FDN + early ref | Very good | Jot 1992, Schlecht & Habets 2017 |
| Analog Variance | Deterministic per-voice hash | Good | Prophet-5/Juno tolerances |
| Octaver | Zero-crossing flip-flop + LP | Good | Boss OC-2 topology |
| Tremolo | Volume LFO (sine/square/tri) | Good | Fender amp tremolo |
| Wah / Auto-Wah | SVF bandpass + LFO/envelope | Good | Cry Baby / Mutron III |
| Ring Mod (effect) | Carrier sine × signal | Good | Moog MF-102 |
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
3. **Reverb sample rate scaling** — Both Schroeder and FDN delay lengths are hardcoded for 44100 Hz. At other sample rates, lengths should be scaled proportionally.
4. **Octaver** — Octave-up mode via full-wave rectification, tracking filter for polyphonic input.
5. **Wah** — Manual/expression modes, sidechain LP mode alongside bandpass.
6. **Ring Mod** — LFO on carrier frequency, square/triangle carrier options.
