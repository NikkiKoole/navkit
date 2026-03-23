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
- [x] **VoicForm** (`WAVE_VOICFORM`) — 4-formant voice with LF glottal pulse, phoneme interpolation, consonants. Upgrade from WAVE_VOICE.
- [x] **Guitar** (`WAVE_GUITAR`) — KS plucked string + pick position comb + 4 biquad body resonance modes. Presets include acoustic, banjo, sitar (jawari bridge buzz), oud.

## TODO — Waveguide / Physical

- [ ] **Saxofony** — conical bore reed with 2 delay lines (vs reed's single bore). Better sax model: the conical bore acts as two coupled waveguides with different impedances. Would replace the sax presets on WAVE_REED or become WAVE_SAX. Ref: STK Saxofony, Cook Ch.10.
- [ ] **BlowHole** — clarinet with register hole + tonehole scattering junctions. Enables overblowing (12th for clarinet, octave for sax) and more realistic fingering transitions. Could extend WAVE_REED with a `registerHole` param. Ref: STK BlowHole, Scavone thesis.
- [x] **StifKarp** (`WAVE_STIFKARP`) — stiff string with allpass dispersion chain for inharmonicity. 8 presets: Grand Piano, Bright Piano, Harpsichord, Dulcimer, Clavichord, Prepared Piano, Honky Tonk, Celesta. Ref: STK StifKarp, Jaffe & Smith.
- [x] **Sitar** — covered by `WAVE_GUITAR` sitar preset with amplitude-dependent delay modulation for jawari bridge buzz. Ref: STK Sitar, Valimaki.
- [x] **Mandolin** (`WAVE_MANDOLIN`) — paired course strings with 3-mode body resonator. Two independent KS delay lines detuned by configurable cents (typ. 1-4) + 3 parallel biquad body modes + pick position + DC blocker. 4 presets: Neapolitan, Flatback, Bouzouki, Charango. Ref: STK Mandolin, Smith.
- [x] **BandedWG** (`WAVE_BANDEDWG`) — banded waveguide synthesis with 4 bandpass-filtered delay lines per mode, bow friction + strike excitation. 6 presets: glass harmonica, singing bowl, bowed vibraphone, wine glass, prayer bowl, tubular chime. Reuses ksBuffer[2048] — zero additional memory. Ref: STK BandedWG, Essl & Cook "Banded Waveguides" (ICMC 1999).
- [x] **Whistle** (`WAVE_WHISTLE`) — Cook pea whistle model: sine oscillator (primary tone) + 2D pea-in-can physics simulation for amplitude/frequency modulation + additive noise for breath texture. Pea bounces off circular can wall with gravity and energy loss, creating characteristic trill. 4 presets: Referee (strong trill), Slide (minimal pea, pitch LFO), Train (breathy, slow attack), Cuckoo (near-pure tone). Ref: STK Whistle, Cook.

## TODO — Collision / Particle

- [x] **Shakers** (`WAVE_SHAKER`) — collision-based particle model (PhISM). Statistical collision engine with SVF resonator bank. 8 presets: maraca, cabasa, tambourine, sleigh bells, bamboo, rain stick, guiro (periodic scrape mode), sandpaper. Ref: STK Shakers, Cook "Physically Informed Sonic Modeling" (PhISM, 1997).

## Implemented — Voice / Formant

- [x] **VoicForm** (`WAVE_VOICFORM`) — 4-formant voice with LF glottal pulse, full phoneme table, smooth phoneme interpolation, consonant/aspiration noise. Text-to-speech UI with ARPABET parser. Ref: STK VoicForm, Cook "Singing Voice Synthesis" (2001).

## TODO — Resonance

- [ ] **Resonate** — generic resonance model with noise/impulse excitation. A 2-pole resonator driven by filtered noise. Simple but useful for wind, thunder, ambient drones, and as a building block. Ref: STK Resonate.

## Priority Ranking

Completed (10 of 13):
1. ~~**BandedWG**~~ — ✅ DONE (WAVE_BANDEDWG, 6 presets)
2. ~~**Shakers**~~ — ✅ DONE (WAVE_SHAKER, 8 presets)
3. ~~**StifKarp**~~ — ✅ DONE (WAVE_STIFKARP, 8 presets)
4. ~~**Sitar**~~ — ✅ DONE (WAVE_GUITAR sitar preset)
5. ~~**VoicForm**~~ — ✅ DONE (WAVE_VOICFORM + text-to-speech UI)
6. ~~**Guitar**~~ — ✅ DONE (WAVE_GUITAR, acoustic/banjo/sitar/oud)
7. ~~**Reed/Clarinet**~~ — ✅ DONE (WAVE_REED, 6 presets)
8. ~~**Brass**~~ — ✅ DONE (WAVE_BRASS, 6 presets)
9. ~~**Mandolin**~~ — ✅ DONE (WAVE_MANDOLIN, 4 presets)
10. ~~**Whistle**~~ — ✅ DONE (WAVE_WHISTLE, 4 presets)

Remaining TODO (low priority — diminishing returns):
1. **Saxofony** — conical bore for better sax (marginal over WAVE_REED sax presets)
2. **BlowHole** — register hole overblowing for reed (marginal)
3. **Resonate** — generic resonator, utility (low)
