# STK-Style Physical Modeling Engines — TODO

Inventory of synthesis engines based on STK (Synthesis Toolkit) and related physical modeling literature. Checked items are already implemented in PixelSynth.

## Implemented

- [x] **Clarinet / Reed** (`WAVE_REED`) — single-reed waveguide, bore LP, reed reflection table. 6 presets: clarinet, soprano/alto/tenor sax, oboe, harmonica.
- [x] **Brass** (`WAVE_BRASS`) — lip-valve waveguide, memoryless tanh nonlinearity, bore LP + mute. 6 presets: trumpet, muted trumpet, trombone, French horn, tuba, flugelhorn.
- [x] **Flute / Blown Pipe** (`WAVE_PIPE`) — jet-drive model (Fletcher/Verge), bore waveguide + tanh jet deflection. 2 presets: pipe flute, recorder.
- [x] **Bowed String** (`WAVE_BOWED`) — Smith/McIntyre dual waveguide, stick-slip friction. 2 presets: cello, fiddle.
- [x] **Plucked String** (`WAVE_PLUCK`) — Karplus-Strong + Thiran allpass fractional delay. Used in pluck presets.
- [x] **Modal Bar / Mallet** (`WAVE_MALLET`) — 4-mode modal synthesis with bar ratios. 5 presets: marimba, vibes, xylophone, glockenspiel, tubular bells.
- [x] **Membrane** (`WAVE_MEMBRANE`) — 6-mode Bessel membrane with pitch bend. 5 presets: tabla, conga, bongo, djembe, tom.
- [x] **Voice / Formant** (`WAVE_VOICE`) — source-filter glottal model + 3 Chamberlin SVF formants. 5 vowels.

## TODO — Waveguide / Physical

- [ ] **Saxofony** — conical bore reed with 2 delay lines (vs reed's single bore). Better sax model: the conical bore acts as two coupled waveguides with different impedances. Would replace the sax presets on WAVE_REED or become WAVE_SAX. Ref: STK Saxofony, Cook Ch.10.
- [ ] **BlowHole** — clarinet with register hole + tonehole scattering junctions. Enables overblowing (12th for clarinet, octave for sax) and more realistic fingering transitions. Could extend WAVE_REED with a `registerHole` param. Ref: STK BlowHole, Scavone thesis.
- [x] **StifKarp** (`WAVE_STIFKARP`) — stiff string with allpass dispersion chain for inharmonicity. 8 presets: Grand Piano, Bright Piano, Harpsichord, Dulcimer, Clavichord, Prepared Piano, Honky Tonk, Celesta. Ref: STK StifKarp, Jaffe & Smith.
- [ ] **Sitar** — plucked string with sympathetic string resonance + jawari bridge buzz (nonlinear bridge termination). The bridge buzz is key — a conditional reflection that activates when amplitude exceeds a threshold. Ref: STK Sitar, Valimaki.
- [ ] **Mandolin** — paired strings (course) with body resonance filter. Two coupled KS delay lines detuned by ~2 cents + body impulse response (short FIR or modal filter). Ref: STK Mandolin, Smith.
- [x] **BandedWG** (`WAVE_BANDEDWG`) — banded waveguide synthesis with 4 bandpass-filtered delay lines per mode, bow friction + strike excitation. 6 presets: glass harmonica, singing bowl, bowed vibraphone, wine glass, prayer bowl, tubular chime. Reuses ksBuffer[2048] — zero additional memory. Ref: STK BandedWG, Essl & Cook "Banded Waveguides" (ICMC 1999).
- [ ] **Whistle** — referee/sports whistle. Helmholtz resonator + turbulent noise excitation. Simple but fun for game SFX. Ref: STK Whistle, Cook.

## TODO — Collision / Particle

- [x] **Shakers** (`WAVE_SHAKER`) — collision-based particle model (PhISM). Statistical collision engine with SVF resonator bank. 8 presets: maraca, cabasa, tambourine, sleigh bells, bamboo, rain stick, guiro (periodic scrape mode), sandpaper. Ref: STK Shakers, Cook "Physically Informed Sonic Modeling" (PhISM, 1997).

## TODO — Voice / Formant

- [ ] **VoicForm** — 4-formant voice with phoneme interpolation. Upgrade to WAVE_VOICE: 4 formants instead of 3, full phoneme table (not just 5 vowels), smooth transitions between phonemes, aspiration noise. Could add consonants, sibilants. Ref: STK VoicForm, Cook "Singing Voice Synthesis" (2001).

## TODO — Resonance

- [ ] **Resonate** — generic resonance model with noise/impulse excitation. A 2-pole resonator driven by filtered noise. Simple but useful for wind, thunder, ambient drones, and as a building block. Ref: STK Resonate.

## Priority Ranking

High impact (unique sounds not covered by existing engines):
1. ~~**BandedWG**~~ — ✅ DONE (WAVE_BANDEDWG, 6 presets)
2. ~~**Shakers**~~ — ✅ DONE (WAVE_SHAKER, 8 presets)
3. ~~**StifKarp**~~ — ✅ DONE (WAVE_STIFKARP, 8 presets)
4. **Sitar** — unique character, bridge buzz is novel

Medium impact (improve existing sounds):
5. **VoicForm** — upgrade WAVE_VOICE with phonemes
6. **Saxofony** — better sax than current reed engine
7. **BlowHole** — overblowing for reed instruments

Lower priority (niche):
8. **Mandolin** — nice but similar to pluck
9. **Whistle** — fun SFX, very simple to add
10. **Resonate** — utility, not a full instrument
