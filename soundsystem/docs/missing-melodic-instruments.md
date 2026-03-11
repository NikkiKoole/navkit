# Missing Melodic Instruments

Status: **Phase 0 DONE** — cataloguing melodic sounds needed for the target aesthetic
(indie, lo-fi, jangle, jazz, SNES/RPG, game music) and how to build them.

All instruments here are SynthPatch presets — no engine changes needed unless noted.

---

## What Exists (98 presets, indices 0-97)

### By category

**Synth leads/bass (electronic):**
Chip Lead, Fat Bass, Wobble, 8bit Pluck, Acid, Power Arp, Warm Tri Bass, Tri Sub

**Pads/atmosphere:**
Arp Pad, Strings, Lush Strings, Mac Juno, Mac Vox, Dark Choir, Eerie Vowel

**Keys:**
Chip Organ, Lofi Keys, Dark Organ, Mac Keys

**Acoustic-ish:**
Pluck (Karplus-Strong), Mac Jangle, Mac Bass, Mac Vibes, Warm Pluck

**Character/game:**
Piku Accord, Piku Tuba, Piku Bell, Piku Glock, Piku Piano, Piku Pluck

**Drums:** 14 presets (808 kit, CR-78 kit, clave, maracas)

### By synthesis engine usage

| Engine | Used by presets | Potential |
|--------|----------------|-----------|
| Square/Saw/Triangle | Chip Lead, Fat Bass, Wobble, Acid, etc. | Covered well |
| FM | Mac Keys, 808 drums | **Underused for melodic** — only 1 melodic preset |
| Pluck (Karplus-Strong) | Pluck, Mac Jangle | Could do much more |
| Additive | (none as preset) | **Completely unused** despite having organ/bell/strings/brass/choir modes |
| Mallet | Marimba, Mac Vibes | **Only 2 of 5 mallet types** have presets |
| SCW (wavetable) | (none as preset) | **Completely unused** — powerful if tables are loaded |
| Granular | (none as preset) | **Completely unused** |
| Voice (formant) | Mac Vox, Eerie Vowel, Dark Choir | 3 presets, could do more |
| Phase Distortion | (none as preset) | **Completely unused** despite 8 waveform modes |
| Membrane | (none melodic) | Only used for drums — but tabla is melodic! |
| Bird | (none as preset) | Niche but useful for game ambience |

**Update:** Phase 0 complete — Additive (Choir Pad, Brass Stab, Add Strings, Add Bell),
Phase Distortion (PD Bass, PD Lead), Mallet (Glockenspiel, Xylophone, Tubular Bells),
Membrane (Tabla), and Bird (Bird Song) all have presets now. SCW and Granular still
have zero presets.

---

## What's Missing

### Tier 1: Core sounds for target genres

These are the instruments you'd reach for when making indie/lo-fi/jazz/SNES music.

#### Keys & Piano

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Rhodes / EP** | FM | *The* indie/lo-fi/jazz keyboard sound. FM with modRatio ~1.0, modIndex 1.0-2.5, bell-like decay. Mac Keys is close but needs variants: mellow (low index), bright (high index), tremolo. |
| **Wurlitzer** | FM or PD | Grittier than Rhodes. More bark, less bell. PD resonant modes could nail this. |
| **Clavinet** | Pluck or PD | Funky, percussive. Filtered Karplus-Strong with short decay and wah-style filter envelope. Essential for funk patterns. |
| **Honky-tonk Piano** | FM + detuned | SNES staple. Two slightly detuned FM voices. Think Chrono Trigger shops. |
| **Celesta / Music Box** | Mallet (glocken) or FM | High register, bell-like, short. SNES RPG item jingles, ice levels. Glocken preset exists but no *preset entry* for it. |
| **Toy Piano** | FM or Square | Slightly detuned, bright, short decay. Lo-fi character sound. |

#### Guitar-like

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Clean Guitar** | Pluck | Mac Jangle exists but is the only guitar sound. Need: nylon (warmer, lower brightness), jazz (darker, rounder), 12-string (double pluck with slight detune). |
| **Muted Guitar** | Pluck | Very short damping, percussive. Funk/reggae rhythm guitar. |
| **Acoustic Strum** | Pluck + arp | Strum chord = rapid arp of plucked notes (up or down). The arp system already supports this — just needs a preset with ARP_RATE_1_32 or FREE at fast rate. |
| **Steel Drum** | FM or Mallet | Caribbean/game music. FM with specific ratios or mallet with bright settings. |

#### Bass

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Upright Bass** | Pluck | Jazz essential. Karplus-Strong with low brightness, medium damping, slight pitch slide on attack. Walking bass lines. |
| **Fretless Bass** | Saw + glide | Mac DeMarco vibe. Mono mode with portamento, warm filter, slight vibrato. |
| **Sub Bass** | Sine/Triangle | Clean low end for lo-fi/house. Pure fundamental, minimal harmonics. |
| **FM Bass** | FM | Punchy, modern. Low mod ratio, high index with fast decay. DX7 bass. |
| **Slap Bass** | Pluck + noise | Funk. Karplus-Strong with noise burst on attack (noiseMix for transient only). |

#### Winds & Brass

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Flute** | Triangle + noise | SNES/RPG staple (Chrono Trigger, Secret of Mana). Triangle wave with breathy noise layer (noiseMix ~0.1), slow attack, vibrato. Or Voice engine with high breathiness. |
| **Recorder** | Square (25% PW) | Medieval/fantasy RPG. Narrow pulse wave with gentle filter, light vibrato. |
| **Clarinet** | Square + filter | Odd harmonics only (square wave characteristics). Warm, woody. Additive with odd-harmonic preset would be more accurate. |
| **Trumpet (muted)** | Saw + filter env | Jazz. Saw with bandpass filter, envelope opens and closes. Or Voice engine. |
| **Ocarina** | Triangle or Sine | Zelda-esque. Pure, hollow. Triangle with very gentle filter, lots of vibrato. |
| **Pan Flute** | Sine + noise | World/ambient. Breathy sine with noise layer, different pitch per "pipe". |
| **Accordion** | Square + detuned | Musette tuning (two detuned voices). Unison=2 with specific detune. Folk/game music. |
| **Harmonica** | Saw + filter | Blues/folk. Saw wave through narrow bandpass. Or Voice engine with specific formants. |

#### Bells & Metallic

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Kalimba / Thumb Piano** | Mallet or Pluck | Warm, short, slightly buzzy. Lo-fi staple. Mallet with high damping or Pluck with medium brightness. |
| **Glockenspiel** | Mallet (glocken) | Preset engine exists (MALLET_PRESET_GLOCKEN) but no instrument_presets.h entry! Free preset. |
| **Xylophone** | Mallet (xylo) | Same — MALLET_PRESET_XYLOPHONE exists, no preset entry. |
| **Tubular Bells** | Mallet (tubular) | Same — MALLET_PRESET_TUBULAR exists, no preset entry. |
| **Chime / Wind Chime** | FM | High-frequency FM with slow random pitch drift. Ambient game audio. |
| **Gamelan** | FM or Mallet | Metallic, inharmonic. FM with non-integer ratio (~1.4) or additive with bell preset. |

#### Pads & Texture

| Instrument | Engine | Why it matters |
|-----------|--------|---------------|
| **Warm Pad** | Saw + unison | Long attack/release, low cutoff, slow filter LFO. The "default pad" that every synth needs. |
| **Glass Pad** | Additive or FM | Bright, crystalline, evolving. Additive with shifting harmonics or FM with slow mod index envelope. |
| **Grain Pad** | Granular | Textural, frozen, evolving. Granular engine exists but has zero presets — it's invisible. |
| **Tape Pad** | Saw + tape FX | Warped, wobbly, degraded. Saw pad through tape saturation with wow/flutter. |
| **Dark Drone** | Square | Low cutoff, high resonance, long everything. Horror/tension game audio. |
| **Choir Pad** | Additive (choir) or Voice | Additive has a choir preset mode but no instrument preset entry. |

#### SNES-Specific

The SNES (SPC700) used 8 BRR-compressed samples at 32kHz, 8 voices, with ADSR + pitch
envelope + echo. Its distinctive sound comes from:
- **Low sample rates** (aliasing/grunge)
- **Limited polyphony** (4-8 voices)
- **BRR compression artifacts** (crunchy)
- **Gaussian interpolation** (slightly muffled)

We can't perfectly replicate BRR artifacts, but the *musical vocabulary* of SNES games
is very achievable with existing engines:

| SNES Sound | Engine | Examples |
|-----------|--------|---------|
| **SNES Strings** | Saw + low filter | Chrono Trigger, FF6. Warm but slightly crunchy. Bitcrusher effect gets close. |
| **SNES Brass** | Square or Saw | Short stabs. Secret of Mana fanfares. |
| **SNES Choir** | Voice or Additive | "Ahh" pad, breathy. FF6 opera, Chrono Trigger 600AD. |
| **SNES Piano** | FM | Slightly metallic, short. Most JRPGs. |
| **SNES Harp** | Pluck | Bright pluck with longer decay. Zelda, Terranigma. |
| **SNES Flute** | Triangle + noise | See flute above. Chrono Trigger wind scene. |
| **SNES Bell** | FM or Mallet | Item pickup, menu select. Very short FM bell. |

These are really just preset authoring with the bitcrusher effect dialed in for crunch.

---

## Priority by Genre Need

What each target genre most urgently needs:

| Genre | Top 3 missing sounds |
|-------|---------------------|
| **Jazz** | Upright Bass, Rhodes, Muted Trumpet |
| **Brush Jazz** | Upright Bass, Rhodes (mellow), Flute |
| **Lo-fi / Bedroom** | Rhodes, Fretless Bass, Kalimba |
| **Jangle / Indie** | Clean Guitar (more variants), Organ (drawbar), Clavinet |
| **SNES / RPG** | Flute, Strings (crunchy), Harp, Music Box |
| **Bossa Nova** | Nylon Guitar, Flute, Upright Bass |
| **Funk** | Clavinet, Slap Bass, Muted Trumpet |
| **Game SFX** | Already covered (Piku presets), but Ocarina, Pan Flute for world-building |

---

## Free Presets (Zero Effort)

These engines/modes already exist but have no preset entry. Literally just adding
an `instrumentPresets[N]` block:

| Sound | Engine | Just needs |
|-------|--------|-----------|
| Glockenspiel | MALLET_PRESET_GLOCKEN | Preset entry |
| Xylophone | MALLET_PRESET_XYLOPHONE | Preset entry |
| Tubular Bells | MALLET_PRESET_TUBULAR | Preset entry |
| Choir Pad | ADDITIVE_PRESET_CHOIR | Preset entry + ADSR |
| Brass Stab | ADDITIVE_PRESET_BRASS | Preset entry + short ADSR |
| String Ensemble | ADDITIVE_PRESET_STRINGS | Preset entry (different from existing saw-based "Strings") |
| Bell | ADDITIVE_PRESET_BELL | Preset entry |
| PD Bass | WAVE_PD (any of 8 modes) | Preset entry — entire CZ-style engine unused! |
| PD Lead | WAVE_PD (reso modes) | Screaming CZ-101 leads, zero presets |
| Tabla (melodic) | WAVE_MEMBRANE | Preset entry — melodic tabla is a real instrument |
| Bird Ambience | WAVE_BIRD | Preset entry — 6 bird types, none exposed |

That's **11 free presets** from engines that were fully implemented but invisible.
**All 11 now implemented** as presets 87-97.

---

## Instruments That Need Engine Work

A few sounds can't be done well with existing engines:

| Sound | What's missing | Effort |
|-------|---------------|--------|
| **Bowed String** | Continuous excitation model (bow pressure/speed). Pluck is impulse-only. | New engine (~200 lines) — listed in roadmap §5.8 |
| **Blown Pipe / Flute (physical)** | Breath noise + resonant tube. Triangle+noise is an approximation. | New engine (~150 lines) — listed in roadmap §5.8 |
| **True Piano** | Requires multi-sample or waveguide model. FM piano is an approximation. | Complex — SCW with piano samples is the practical path |
| **Distortion Guitar** | Need waveshaping beyond tanh (wavefold helps). Existing drive is too clean. | Wavefold (§1 in synthesis-additions.md) would unlock this |

These are **not blockers** — FM/Pluck/Additive approximations work fine for the
lo-fi/SNES aesthetic where "perfectly accurate" isn't the goal.

---

## Implementation Plan

| Phase | What | Count | Status |
|-------|------|-------|--------|
| **0: Free presets** | Glocken, Xylo, Tubular, Choir, Brass, Strings, Bell, PD×2, Tabla, Bird | 11 | **DONE** (presets 87-97) |
| **1: Core keys** | Rhodes (×2), Wurlitzer, Clavinet, Celesta, Toy Piano | 6 | TODO |
| **2: Core bass** | Upright, Fretless, Sub, FM Bass, Slap | 5 | Partial — Sub Bass (61), Nylon Guitar (62) done |
| **3: Guitar variants** | Nylon, Jazz, Muted, 12-string | 4 | Partial — Nylon Guitar (62) done |
| **4: Winds** | Flute, Recorder, Ocarina, Muted Trumpet, Accordion | 5 | TODO |
| **5: World/bells** | Kalimba, Steel Drum, Gamelan, Pan Flute, Harmonica | 5 | Partial — Kalimba (60) done |
| **6: SNES kit** | SNES Strings, Brass, Choir, Piano, Harp, Bell (with bitcrusher) | 6 | TODO |
| **7: Pads** | Warm, Glass, Grain, Tape, Drone | 5 | Partial — WC Pad (82) done |

Phase 0 is complete — all 5 previously-unused synthesis engines now have presets.
Also added 6 West Coast showcase presets (81-86) using wavefold/ring mod/hard sync.
Total presets: 98 (up from 48).
