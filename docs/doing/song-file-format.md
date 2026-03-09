# Song File Format & DAW Save/Load

Status: **in progress** — Phases 1-3 done, Phase 4+ remaining

## Source Files Map

Read these files to understand the full system before making changes.

### Synth Engine (header-only, all real-time DSP)
| File | What it is | Lines |
|---|---|---|
| `soundsystem/engines/synth.h` | Core synth: 14 oscillator types, 16-voice polyphony, ADSR, filters, LFOs, arpeggiator, unison, scale lock. All `note*` globals, `playNote/playFM/playMallet/etc.` | ~3400 |
| `soundsystem/engines/sequencer.h` | Step sequencer: Pattern struct (drum+melody+plocks+note pools), DrumSequencer, 96 PPQ timing, trigger callbacks, sound log | ~1700 |
| `soundsystem/engines/drums.h` | Drum machine: 16 synth drum types (808+CR78), DrumParams, per-voice processing | ~830 |
| `soundsystem/engines/effects.h` | Effects: master (dist/delay/tape/crush/chorus/reverb/sidechain), per-bus (7 buses: filter/dist/delay/reverb send), dub loop, rewind | ~1780 |
| `soundsystem/engines/sampler.h` | WAV sample playback (up to 32 slots) | |
| `soundsystem/engines/rhythm_patterns.h` | Classic rhythm pattern generator presets | |
| `soundsystem/engines/piku_presets.h` | Pikuniku-style "sillycore" instrument presets | |
| `soundsystem/soundsystem.h` | Unified context struct: bundles synth + drums + effects + sequencer + sampler into one `SoundSystem` | |

### DAW Tool
| File | What it is |
|---|---|
| `soundsystem/demo/demo.c` | Full interactive DAW: keyboard synth, sequencer grid editor, drum/melody step inspectors, mixer with 7 buses, 6 collapsible param columns, 8 scene slots (RAM-only), speech system. ~4200 lines |

### Game Integration (bridge between soundsystem and navkit)
| File | What it is |
|---|---|
| `src/sound/sound_synth_bridge.h` | Public API: `SoundSynth` struct, `SoundSynthCreate/Destroy`, `SoundSynthPlaySong*`, jukebox functions, `SongPlayer` struct |
| `src/sound/sound_synth_bridge.c` | Implementation: per-song trigger functions (melody callbacks for each instrument), `SongPlayer` pattern cycling, audio callback, all 13 `SoundSynthPlaySong*` functions, jukebox logic |
| `src/sound/songs.h` | 13 songs as static C functions: `Song_*_ConfigureVoices()` + `Song_*_PatternA/B/C()` + helper macros (`note()`, `drum()`, `noteN()`, etc.). ~3600 lines |
| `src/sound/sound_phrase.h` | Procedural phrase/speech token system (SoundToken, SoundPhrase) |
| `src/sound/sound_phrase.c` | Phrase player implementation |

### Tests
| File | What it is |
|---|---|
| `tests/test_soundsystem.c` | 1200+ test assertions: synth voices, sequencer timing, pattern playback, p-locks, note pools, trigger conditions, drum processing, effects, song player |

### Embedded Data (generated, don't hand-edit)
| File | What it is |
|---|---|
| `soundsystem/engines/scw_data.h` | Embedded single-cycle waveforms (NES, SID, birds). Regenerate with `make scw_embed` |
| `soundsystem/engines/sample_data.h` | Embedded WAV samples. Regenerate with `make sample_embed` |
| `soundsystem/tools/scw_embed.c` | Tool: reads `soundsystem/cycles/` WAVs → generates `scw_data.h` |
| `soundsystem/tools/sample_embed.c` | Tool: reads `soundsystem/oneshots/` WAVs → generates `sample_data.h` |

### Documentation
| File | What it is |
|---|---|
| `docs/doing/song-file-format.md` | **This file** — song file format spec, DAW plans, DJ system, iMUSE comparison |
| `docs/doing/midi-to-sequencer.md` | MIDI conversion notes: step/nudge mapping, drum mapping, limitations, songs converted |
| `soundsystem/README.md` | Synth engine overview and API reference |
| `soundsystem/docs/sequencer-engine-report.md` | Sequencer architecture deep-dive |
| `soundsystem/docs/game-integration.md` | How the soundsystem connects to the game |
| `soundsystem/docs/DUB_LOOP.md` | Dub loop (King Tubby tape delay) design notes |
| `soundsystem/docs/roadmap.md` | Soundsystem feature roadmap |
| `soundsystem/docs/next.md` | Near-term soundsystem TODO items |
| `soundsystem/docs/ux-insight.md` | UX design notes for the DAW |
| `soundsystem/IDEAS.md` | Loose ideas and feature brainstorms |

### Related Game Docs (sound in context of gameplay)
| File | What it is |
|---|---|
| `docs/todo/sounds/sound-and-mind-design.md` | Sound + mood/emotion system design |
| `docs/todo/sounds/sound-and-mind-research.md` | Research on sound affecting mover behavior |
| `docs/todo/sounds/sound-and-mind-speech-comm.md` | Speech/communication system design |
| `docs/todo/sounds/sound-and-mind-birdsong-sex.md` | Environmental birdsong + mating calls |
| `docs/todo/sounds/sound-concept-generator.md` | Procedural sound concept generation |
| `docs/todo/sounds/sound-needs-emotions-groundwork.md` | Needs/emotions → sound mapping groundwork |

### Key Structs Quick Reference

| Struct | Defined in | What it holds |
|---|---|---|
| `SynthPatch` | `demo.c:404` | Complete instrument definition: wave type, ADSR, filter, LFOs, arp, unison, pluck/additive/mallet/FM/PD/membrane/granular/voice/bird params |
| `Pattern` | `sequencer.h:261` | One pattern: 4×32 drum steps + 3×32 melody steps + velocities + gates + slides + accents + sustain + note pools + 128 p-locks + track lengths |
| `DrumSequencer` | `sequencer.h:307` | Full sequencer state: 8 patterns, playback positions, trigger callbacks, groove (DillaTiming), humanize, track volumes, flam state |
| `DrumParams` | `drums.h:91` | All drum sound parameters (kick/snare/clap/hat/tom/rim/cowbell/clave/maracas/CR78 variants) |
| `Effects` | `effects.h:189` | Master effects: distortion, delay, tape, bitcrusher, chorus, reverb, sidechain |
| `BusEffects` | `effects.h:123` | Per-bus effects: volume, pan, mute/solo, filter (SVF), distortion, delay, reverb send |
| `Scene` | `demo.c:975` | DAW scene snapshot: 4 patches + drum params + effects + volumes. **Currently missing: patterns, BPM, scale, arrangement** |
| `SongPlayer` | `sound_synth_bridge.h` | Runtime song state: loaded patterns, current pattern/loop, BPM, per-track voice tracking, sweep state |
| `SoundSynth` | `sound_synth_bridge.h` | Top-level game audio: SoundSystem + raylib AudioStream + SongPlayer + PhrasePlayer |
| `NotePool` | `sequencer.h:166` | Per-step chord config: chord type, pick mode, custom notes (up to 8), cycling state |
| `PLock` | `sequencer.h:242` | Single parameter lock: step + track + param + value + linked list pointer |
| `DillaTiming` | `sequencer.h:198` | Drum micro-timing: kick/snare/hat/clap nudge + swing + jitter |

---

## Problem

Three disconnected worlds:

1. **Songs are C code** — `songs.h` has 13 songs as static functions. Can't edit at runtime, can't round-trip.
2. **DAW has no persistence** — scenes save patches/drums/effects to RAM, but no sequencer data, and nothing survives quitting.
3. **MIDI conversion is one-way** — Python script → hand-written C. No way to load the result back into the DAW for tweaking.

Instruments are defined inline per-song (each trigger function sets envelope/filter/wave params). The same "FM Rhodes" is copy-pasted across Jazz, House, Dilla with minor tweaks.

## Design

Three file types, three concerns:

```
.patch  — Single instrument definition (SynthPatch)
.bank   — Collection of named patches (instrument library)
.song   — Full song: arrangement + patterns + mix + embedded or referenced instruments
```

### Format Rules

- `#` starts a line comment (ignored by parser)
- Sections start with `[section]` or `[section.name]`
- Key-value pairs: `key = value` (spaces around `=` optional)
- Event lines (d, m, p, c): space-separated positional fields + optional `key=value` tags
- Omitted keys get engine defaults (safe to leave out anything you don't care about)
- String values with spaces need quotes: `name = "Dormitory Ambient"`
- Enum values use lowercase names: `waveType = fm`, `scaleType = dorian`

---

## Instrument Patch Format

A patch = everything needed to trigger one sound. Maps 1:1 to the `SynthPatch` struct in demo.c. A `.patch` file is one `[patch]` section. A `.bank` file is multiple `[patch "name"]` sections.

```
[patch "warm pluck bass"]

# ---- oscillator ----
# square | saw | triangle | noise | scw | voice | pluck | additive
# mallet | granular | fm | pd | membrane | bird
waveType = pluck
scwIndex = 0              # wavetable index (for scw/granular, 0-255)

# ---- amplitude envelope (ADSR) ----
attack  = 0.01            # seconds (0.001 - 5.0)
decay   = 0.1             # seconds (0.01 - 5.0)
sustain = 0.5             # level (0.0 - 1.0)
release = 0.3             # seconds (0.01 - 10.0)
expRelease = false        # true = exponential (natural tail), false = linear (tight cutoff)
volume  = 0.5             # output level (0.0 - 1.0), scaled by note velocity at trigger

# ---- pulse width modulation (square wave) ----
pulseWidth = 0.5          # duty cycle (0.1 - 0.9)
pwmRate    = 3.0          # LFO rate in Hz
pwmDepth   = 0.0          # modulation depth (0.0 - 0.5)

# ---- vibrato ----
vibratoRate  = 5.0        # Hz
vibratoDepth = 0.0        # semitones (0.0 - 2.0)

# ---- filter (resonant lowpass, per-voice) ----
filterCutoff    = 1.0     # normalized (0.0 - 1.0, 1.0 = fully open)
filterResonance = 0.0     # resonance (0.0 - 1.0)
filterEnvAmt    = 0.0     # envelope → cutoff modulation (-1.0 to 1.0)
filterEnvAttack = 0.01    # filter envelope attack (seconds)
filterEnvDecay  = 0.2     # filter envelope decay (seconds)

# ---- filter LFO ----
filterLfoRate  = 0.0      # Hz (0 = off)
filterLfoDepth = 0.0      # modulation depth (0.0 - 1.0)
filterLfoShape = 0        # 0=sine, 1=triangle, 2=square, 3=saw, 4=sample&hold
filterLfoSync  = 0        # tempo sync division (0=off, 1-7=tempo divisions)

# ---- resonance LFO ----
resoLfoRate  = 0.0        # Hz
resoLfoDepth = 0.0        # depth (0.0 - 1.0)
resoLfoShape = 0          # same shape enum as filter LFO

# ---- amplitude LFO (tremolo) ----
ampLfoRate  = 0.0         # Hz
ampLfoDepth = 0.0         # depth (0.0 - 1.0)
ampLfoShape = 0           # shape enum

# ---- pitch LFO ----
pitchLfoRate  = 0.0       # Hz
pitchLfoDepth = 0.0       # semitones
pitchLfoShape = 0         # shape enum

# ---- arpeggiator ----
arpEnabled = false
arpMode    = 0            # 0=up, 1=down, 2=updown, 3=random
arpRateDiv = 0            # tempo division (0-7)
arpRate    = 8.0          # free rate in Hz (used when arpRateDiv=0)
arpChord   = 0            # 0=octave, 1=major, 2=minor, 3=dom7, 4=min7, 5=sus4, 6=power

# ---- unison ----
unisonCount  = 1          # 1-4 voices
unisonDetune = 10.0       # cents between voices
unisonMix    = 0.5        # blend (0.0 - 1.0)

# ---- mono / glide ----
monoMode  = false         # monophonic mode (one voice, steals)
glideTime = 0.0           # portamento time in seconds

# ---- pluck (Karplus-Strong) ----
pluckBrightness = 0.5     # initial brightness (0.0 - 1.0)
pluckDamping    = 0.5     # string damping (0.0 - 1.0, higher = duller faster)
pluckDamp       = 0.0     # additional damping

# ---- additive synthesis ----
# preset: sine | organ | bell | strings | brass | choir | custom
additivePreset       = organ
additiveBrightness   = 0.5  # harmonic rolloff (0.0 - 1.0)
additiveShimmer      = 0.0  # per-harmonic phase drift (0.0 - 1.0)
additiveInharmonicity = 0.0 # frequency ratio stretch (0.0 - 1.0)

# ---- mallet percussion ----
# preset: marimba | vibes | xylophone | glocken | tubular
malletPreset    = marimba
malletStiffness = 0.5     # bar stiffness (0.0 - 1.0)
malletHardness  = 0.5     # mallet hardness (0.0 - 1.0, soft rubber → hard plastic)
malletStrikePos = 0.5     # where on the bar (0.0 = node, 1.0 = center)
malletResonance = 0.5     # resonator coupling (0.0 - 1.0)
malletTremolo   = 0.0     # motor tremolo depth (0.0 - 1.0, vibes style)
malletTremoloRate = 5.0   # tremolo rate Hz
malletDamp      = 0.0     # damping pedal (0.0 - 1.0)

# ---- voice (formant synthesis) ----
# vowel: a | e | i | o | u
voiceVowel        = 0     # starting vowel (0=A, 1=E, 2=I, 3=O, 4=U)
voiceFormantShift = 0.0   # formant frequency shift (-1.0 to 1.0)
voiceBreathiness  = 0.0   # breath noise amount (0.0 - 1.0)
voiceBuzziness    = 0.5   # buzz/rasp (0.0 - 1.0)
voiceSpeed        = 1.0   # vowel transition speed
voicePitch        = 1.0   # pitch multiplier
voiceConsonant    = false  # consonant attack transient
voiceConsonantAmt = 0.0   # consonant strength (0.0 - 1.0)
voiceNasal        = false  # nasal quality
voiceNasalAmt     = 0.0   # nasality amount (0.0 - 1.0)
voicePitchEnv      = 0.0  # pitch envelope depth
voicePitchEnvTime  = 0.1  # pitch envelope time (seconds)
voicePitchEnvCurve = 0.5  # pitch envelope curve shape

# ---- granular synthesis ----
granularScwIndex   = 0    # source wavetable index (0-255)
granularGrainSize  = 0.05 # grain duration in seconds
granularDensity    = 10.0 # grains per second
granularPosition   = 0.5  # playback position in wavetable (0.0 - 1.0)
granularPosRandom  = 0.0  # position randomization (0.0 - 1.0)
granularPitch      = 1.0  # pitch multiplier
granularPitchRandom = 0.0 # pitch randomization (0.0 - 1.0)
granularAmpRandom  = 0.0  # amplitude randomization (0.0 - 1.0)
granularSpread     = 0.0  # stereo spread (0.0 - 1.0)
granularFreeze     = false # freeze position (looping grains)

# ---- FM synthesis (2-operator) ----
fmModRatio  = 1.0         # modulator:carrier frequency ratio
fmModIndex  = 1.0         # modulation depth (0.0 - 10.0)
fmFeedback  = 0.0         # operator self-feedback (0.0 - 1.0)

# ---- phase distortion (CZ-style) ----
# pdWaveType: saw | square | pulse | doublepulse | sawpulse | reso1 | reso2 | reso3
pdWaveType   = 0          # PD_WAVE_* enum (0-7)
pdDistortion = 0.5        # distortion amount (0.0 - 1.0)

# ---- membrane (pitched drum) ----
# membranePreset: tabla | conga | bongo | djembe | tom
membranePreset   = 0      # MEMBRANE_* enum (0-4)
membraneDamping  = 0.5    # damping (0.0 - 1.0)
membraneStrike   = 0.5    # strike position (0.0 = center, 1.0 = edge)
membraneBend     = 0.0    # pitch bend depth (0.0 - 1.0)
membraneBendDecay = 0.1   # pitch bend decay time (seconds)

# ---- bird vocalization ----
# birdType: chirp | trill | warble | tweet | whistle | cuckoo
birdType       = 0        # BIRD_* enum (0-5)
birdChirpRange = 0.5      # pitch sweep range
birdTrillRate  = 10.0     # trill repetition rate (Hz)
birdTrillDepth = 0.3      # trill pitch depth
birdAmRate     = 5.0      # amplitude modulation rate (Hz)
birdAmDepth    = 0.3      # amplitude modulation depth
birdHarmonics  = 0.3      # harmonic content (0.0 - 1.0)
```

A `.bank` file is just multiple `[patch "name"]` sections concatenated. You only need to specify fields that differ from defaults — omitted fields get engine defaults.

The DAW already has 4 SynthPatch slots (Preview, Bass, Lead, Chord). Saving a patch = serialize that struct. Loading = deserialize into a slot.

---

## Song File Format (`.song`)

A complete, self-contained song. All sections are optional except `[song]`.

```
# ============================================================================
# SONG HEADER
# ============================================================================

[song]
name = "Dormitory Ambient"
bpm = 56.0                # tempo (20.0 - 300.0)
loopsPerPattern = 2       # how many times each arrangement entry loops before advancing
use32ndNoteMode = false   # false = 16 steps/bar (24 ticks/step), true = 32 steps/bar (12 ticks/step)

# ============================================================================
# SCALE LOCK — constrains all midiToFreqScaled() calls to a scale
# ============================================================================

[scale]
enabled = true
root = 2                  # 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B
# type: chromatic | major | minor | pentatonic | minorPentatonic | blues | dorian | mixolydian | harmonicMinor
type = dorian

# ============================================================================
# GROOVE — drum micro-timing and humanization
# ============================================================================

[groove]
# Dilla-style per-drum-track timing offsets (in ticks, 24 ticks = 1 sixteenth step)
kickNudge  = 0            # kick timing offset (negative = early, positive = late)
snareDelay = 0            # snare timing offset
hatNudge   = 0            # hihat timing offset
clapDelay  = 0            # clap timing offset
swing      = 0            # off-beat swing amount in ticks (0-12)
jitter     = 0            # random humanization range in ticks (0-6)

# Melody humanization (applies to all melody tracks)
melodyTimingJitter  = 0   # random timing offset range in ticks (0-6)
melodyVelocityJitter = 0.0 # random velocity variation (0.0 - 0.2)

# ============================================================================
# INSTRUMENTS — per-track synth patches (inline or bank reference)
# ============================================================================
#
# 3 melody tracks: bass (track 0), lead (track 1), chord (track 2)
# Each gets an instrument definition. Two ways to specify:
#
#   bass = inline          → full patch defined in [instrument.bass] below
#   bass = "warm pluck"    → looked up by name from loaded .bank file
#
# If a track has no instrument section and no bank reference,
# it uses a default saw patch.

[instruments]
bass  = inline
lead  = inline
chord = inline

# Full patch definitions (same format as .patch/.bank files, minus the [patch] header)

[instrument.bass]
waveType = triangle
attack = 0.3
decay = 0.6
sustain = 0.4
release = 1.2
volume = 0.5
filterCutoff = 0.25
filterResonance = 0.15

[instrument.lead]
waveType = mallet
malletPreset = glocken
volume = 0.4

[instrument.chord]
waveType = additive
additivePreset = choir
attack = 0.8
decay = 1.0
sustain = 0.5
release = 2.0
volume = 0.35

# ============================================================================
# DRUMS — sound selection + parameter tweaks
# ============================================================================

[drums]
# Which drum sound each of the 4 drum tracks uses.
#
# Synthesized drums:
#   kick | snare | clap | closedHH | openHH | lowTom | midTom | hiTom
#   rimshot | cowbell | clave | maracas
#   cr78Kick | cr78Snare | cr78HH | cr78Metal
#
# Sample-based drums (WAV files loaded via sampler):
#   sample:<slot>     where slot = 0-based index into embedded/loaded samples
#   Examples: sample:0, sample:3, sample:12
#
# The sample slot maps to DRUM_SYNTH_COUNT + slot internally.
# Samples must be loaded (embedded via make sample_embed, or loaded at runtime).
track0 = kick
track1 = snare
track2 = closedHH
track3 = clap

# Drum parameter overrides (optional — omitted values use engine defaults)
# All parameters listed here; include only the ones you want to change.
#
# Kick
kickPitch      = 50.0     # base pitch in Hz (30 - 80)
kickDecay      = 0.5      # decay time in seconds (0.1 - 1.0)
kickPunchPitch = 150.0    # pitch envelope start Hz (80 - 200)
kickPunchDecay = 0.04     # pitch envelope time (0.01 - 0.1)
kickClick      = 0.3      # initial click transient (0.0 - 1.0)
kickTone       = 0.5      # tone/distortion (0.0 - 1.0)
#
# Snare
snarePitch  = 180.0       # tone pitch Hz (100 - 300)
snareDecay  = 0.2         # decay seconds (0.1 - 0.5)
snareSnappy = 0.6         # noise amount (0.0 - 1.0)
snareTone   = 0.5         # tone vs noise balance (0.0 - 1.0)
#
# Clap
clapDecay  = 0.3          # decay seconds
clapTone   = 0.6          # filter brightness (0.0 - 1.0)
clapSpread = 0.012        # timing spread of "hands" (seconds)
#
# Hihat
hhDecayClosed = 0.05      # closed decay seconds (0.02 - 0.15)
hhDecayOpen   = 0.4       # open decay seconds (0.2 - 1.0)
hhTone        = 0.7       # brightness (0.0 - 1.0)
#
# Tom
tomPitch      = 1.0       # pitch multiplier
tomDecay      = 0.3       # decay seconds
tomPunchDecay = 0.05      # pitch envelope time
#
# Rimshot
rimPitch = 1700.0         # click pitch Hz
rimDecay = 0.03           # decay seconds
#
# Cowbell
cowbellPitch = 560.0      # pitch Hz
cowbellDecay = 0.3        # decay seconds
#
# Clave
clavePitch = 2500.0       # pitch Hz
claveDecay = 0.02         # decay seconds
#
# Maracas
maracasDecay = 0.1        # decay seconds
maracasTone  = 0.5        # brightness
#
# CR-78 Kick
cr78KickPitch     = 80.0  # pitch Hz (60 - 100)
cr78KickDecay     = 0.3   # decay seconds
cr78KickResonance = 0.5   # bridged-T resonance
#
# CR-78 Snare
cr78SnarePitch  = 200.0   # resonant ping pitch
cr78SnareDecay  = 0.2     # decay
cr78SnareSnappy = 0.5     # noise amount
#
# CR-78 Hihat
cr78HHDecay = 0.05        # decay
cr78HHTone  = 0.6         # brightness
#
# CR-78 Metal
cr78MetalPitch = 800.0    # pitch Hz
cr78MetalDecay = 0.2      # decay

# ============================================================================
# MIX — volumes per track
# ============================================================================

[mix]
masterVolume = 0.25       # overall output level (0.0 - 1.0)
drumVolume   = 0.6        # drum engine master volume (0.0 - 1.0)

# Per-track volumes (0.0 - 1.0, default 1.0)
# Tracks 0-3 = drum tracks (kick, snare, hihat, clap)
# Tracks 4-6 = melody tracks (bass, lead, chord)
track0 = 1.0
track1 = 1.0
track2 = 0.8
track3 = 0.7
track4 = 1.0
track5 = 0.9
track6 = 0.85

# ============================================================================
# MASTER EFFECTS
# ============================================================================

[effects]
# Distortion
distEnabled = false
distDrive   = 1.0         # drive amount (1.0 - 10.0)
distTone    = 0.5         # post-distortion lowpass (0.0 - 1.0)
distMix     = 0.5         # dry/wet (0.0 - 1.0)

# Delay
delayEnabled  = false
delayTime     = 0.3       # seconds (0.05 - 1.0)
delayFeedback = 0.4       # feedback (0.0 - 0.9)
delayMix      = 0.3       # dry/wet (0.0 - 1.0)
delayTone     = 0.5       # feedback lowpass (0.0 - 1.0)

# Tape saturation
tapeEnabled    = false
tapeWow        = 0.0      # slow pitch wobble (0.0 - 1.0)
tapeFlutter    = 0.0      # fast pitch wobble (0.0 - 1.0)
tapeSaturation = 0.0      # warmth/saturation (0.0 - 1.0)
tapeHiss       = 0.0      # hiss amount (0.0 - 1.0)

# Bitcrusher
crushEnabled = false
crushBits    = 16.0       # bit depth (2 - 16)
crushRate    = 1.0        # sample rate reduction (1 - 32)
crushMix     = 0.5        # dry/wet

# Chorus
chorusEnabled  = false
chorusRate     = 1.0       # LFO rate Hz (0.1 - 5.0)
chorusDepth    = 0.5       # modulation depth (0.0 - 1.0)
chorusMix      = 0.3       # dry/wet
chorusDelay    = 0.01      # base delay seconds (0.005 - 0.030)
chorusFeedback = 0.0       # feedback for flanging (0.0 - 0.5)

# Reverb (Schroeder)
reverbEnabled  = false
reverbSize     = 0.5       # room size (0.0 - 1.0)
reverbDamping  = 0.5       # HF damping (0.0 - 1.0)
reverbMix      = 0.2       # dry/wet
reverbPreDelay = 0.0       # pre-delay seconds (0.0 - 0.1)

# Sidechain compression
sidechainEnabled = false
sidechainSource  = 0       # 0=kick, 1=snare, 2=clap, 3=hihat, 4=allDrums
sidechainTarget  = 0       # 0=bass, 1=lead, 2=chord, 3=all
sidechainDepth   = 0.5     # duck amount (0.0 - 1.0)
sidechainAttack  = 0.01    # seconds (0.001 - 0.05)
sidechainRelease = 0.15    # seconds (0.05 - 0.5)

# Dub loop (King Tubby-style tape delay)
dubEnabled    = false
dubTime       = 0.5        # loop/head time in seconds (0.05 - 4.0)
dubFeedback   = 0.5        # echo feedback (0.0 - 0.95)
dubMix        = 0.3        # dry/wet (0.0 - 1.0)
dubInputSource = 0         # 0=all, 1=drums, 2=synth, 3=manual,
                           # 4-15=individual drums, 16-18=individual synths,
                           # 24-30=post-bus outputs
dubPreReverb  = false      # true = signal→reverb→delay, false = signal→delay→reverb
dubSaturation = 0.0        # tape saturation per pass (0.0 - 1.0)
dubToneHigh   = 0.5        # HF loss per pass (0.0 - 1.0, lower = darker echoes)
dubToneLow    = 0.0        # LF loss per pass (0.0 - 1.0)
dubNoise      = 0.0        # tape hiss per pass (0.0 - 1.0)
dubDegradeRate = 0.0       # cumulative degradation per echo (0.0 - 1.0)
dubWow        = 0.0        # slow speed wobble (0.0 - 1.0)
dubFlutter    = 0.0        # fast speed wobble (0.0 - 1.0)
dubSpeed      = 1.0        # playback speed (0.5 = octave down, 2.0 = octave up)

# ============================================================================
# PER-BUS EFFECTS (optional — 7 buses: drum0-3, bass, lead, chord)
# ============================================================================
# Bus names: drum0 | drum1 | drum2 | drum3 | bass | lead | chord
# Omitted buses use engine defaults (no effects, volume 1.0, center pan)

[bus.bass]
volume    = 1.0            # bus volume (0.0 - 2.0, 1.0 = unity)
pan       = 0.0            # stereo pan (-1.0 left to 1.0 right)
mute      = false
solo      = false

# Bus filter (state variable)
filterEnabled   = true
filterCutoff    = 0.4      # normalized (0.0 - 1.0)
filterResonance = 0.0      # resonance (0.0 - 1.0)
# filterType: lowpass | highpass | bandpass
filterType      = lowpass

# Bus distortion
distEnabled = false
distDrive   = 1.0
distMix     = 0.5

# Bus delay
delayEnabled   = false
delayTime      = 0.3       # seconds (or sync division if tempoSync)
delayFeedback  = 0.4
delayMix       = 0.3
delayTempoSync = false     # use sync division instead of time
# delaySyncDiv: 16th | 8th | quarter | half | bar
delaySyncDiv   = 0

# Bus reverb send (to master reverb)
reverbSend = 0.0           # send amount (0.0 - 1.0)

# ============================================================================
# SONG METADATA — tells the game when/how to use this song
# ============================================================================

[metadata]
author = ""                # composer / arranger name
description = ""           # short description of the song

# Mood tags — the game uses these to pick songs for situations.
# Comma-separated list from: calm, tense, melancholy, hopeful, playful,
# mysterious, triumphant, peaceful, urgent, dreamy, warm, dark, upbeat
mood = calm, dreamy

# Energy level: low | medium | high
# Low = ambient/sleep, medium = working/exploring, high = combat/celebration
energy = low

# Game context hints (comma-separated):
#   dormitory, workshop, outdoors, exploration, combat, menu,
#   rain, night, celebration, death, any
context = dormitory, night

# ============================================================================
# SONG TRANSITIONS — how the game fades in/out
# ============================================================================

[transitions]
fadeIn  = 2.0              # fade in duration in seconds (0 = instant start)
fadeOut = 3.0              # fade out duration in seconds (0 = instant stop)
crossfade = true           # allow crossfade overlap with previous/next song

# ============================================================================
# ARRANGEMENT — named sections with pattern playback order
# ============================================================================

[arrangement]
# Each entry: optional_name:pattern_index
# Names are for readability + game hooks (e.g., skip "intro" on repeat plays).
# If no name needed, just use the pattern index.
#
# The song player plays entries in sequence, looping each `loopsPerPattern`
# times before advancing. After the last entry, loops back to the start.
#
# Examples:
#   order = 0                                       # loop one pattern forever
#   order = intro:0, verse:1, chorus:2              # named sections
#   order = intro:0, verse:1, verse:1, chorus:2, bridge:3, verse:1, chorus:2, outro:4
#   order = A:0, A:0, B:1, A:0                     # AABA jazz form

order = intro:0, main:1

# ---- transition patterns (iMUSE-style bridge pieces) ----
# When the DJ jumps between named sections, it can play a short
# transitional pattern to bridge the harmonic/rhythmic gap.
# Format: transition.<from>_to_<to> = <pattern_index>
#
# If no transition is defined for a given pair, the jump is direct.
#
# transition.explore_to_combat = 5      # pattern 5 bridges explore→combat
# transition.combat_to_explore = 6      # pattern 6 bridges combat→explore

# ---- stinger patterns (one-shot overlays) ----
# Short musical phrases that play over the current song without
# interrupting the arrangement. Used for event feedback:
# victory fanfare, danger alert, discovery chime, etc.
#
# Stingers use pattern data but are NOT part of the arrangement order.
# The DJ triggers them: DJ_PlayStinger("victory")
#
# [stinger.victory]
# melodyTrackLength = 4, 4, 4
# m 1 0 C5 0.9 1
# m 1 1 E5 0.9 1
# m 1 2 G5 0.85 2
#
# [stinger.danger]
# drumTrackLength = 4, 4, 4, 4
# d 0 0 0.95
# d 1 2 0.80
# m 0 0 C2 0.7 4

# ============================================================================
# PATTERNS — sequence data (up to 8 patterns, indexed 0-7)
# ============================================================================

[pattern.0]

# ---- track lengths (default 16) ----
# Use shorter lengths for odd time signatures (12 = 3/4) or polyrhythm.
# Each track can have a different length.
drumTrackLength   = 16, 16, 16, 16    # kick, snare, hihat, clap
melodyTrackLength = 16, 16, 16         # bass, lead, chord

# ---- per-pattern overrides (optional) ----
# Any of these override the song-level defaults for this pattern only.
# Omitted = inherit from song globals. This is how sections change character.
#
# scaleRoot = 7              # modulate to G for the chorus
# scaleType = major          # switch from minor to major
# bpm = 130.0                # tempo change for this section
# use32ndNoteMode = true     # override resolution for this pattern (e.g., grace notes)
#
# instrument.bass = "saw bass"        # switch bass instrument (bank ref)
# instrument.bass = inline            # or define inline in [pattern.0.instrument.bass]
# instrument.lead = inline
#
# muteTrack = 0, 3           # mute kick and clap (breakdown / drop)
# soloTrack = 4              # solo bass only
#
# groove.swing = 9           # heavier swing for this section
# groove.jitter = 4          # drunker feel

# ---- drum events ----
# Format: d <track> <step> <velocity> [key=value ...]
#
#   track:    0-3 (0=kick, 1=snare, 2=hihat, 3=clap — or whatever [drums] assigns)
#   step:     0-31 (0-based, must be < track length)
#   velocity: 0.0 - 1.0
#
# Optional tags (in any order after velocity):
#   pitch=<float>       pitch offset (-1.0 to 1.0, 0.0 = normal)
#   prob=<float>        trigger probability (0.0 - 1.0, default 1.0 = always)
#   cond=<name>         trigger condition:
#                         always | 1:2 | 2:2 | 1:4 | 2:4 | 3:4 | 4:4
#                         fill | notFill | first | notFirst

d 0  0 0.90
d 0  8 0.85
d 2  2 0.15
d 2  6 0.12
d 2 10 0.15
d 2 14 0.10
d 3 12 0.08 pitch=0.3 prob=0.4

# ---- melody events ----
# Format: m <track> <step> <note> <velocity> <gate> [key=value ...]
#
#   track:    0-2 (0=bass, 1=lead, 2=chord)
#   step:     0-31
#   note:     MIDI note name or number. Names: C0-G9, with # or b for accidentals.
#               Examples: C4, D#3, Eb2, A4, 60
#               REST or - for a rest (explicit silence, clears held note)
#   velocity: 0.0 - 1.0
#   gate:     length in steps (1-32). How long the note sounds.
#               0 = legato/tie (note rings until next note on same track)
#
# Optional tags (in any order after gate):
#   nudge=<float>       timing offset in ticks (-12 to 12 in 16th mode, -6 to 6 in 32nd)
#   sustain=<int>       extra hold steps after gate (0 = off). Note holds until
#                         gate+sustain steps pass, even without a new trigger.
#   slide               portamento/glide into this note from previous
#   accent              velocity boost + filter envelope boost
#   prob=<float>        trigger probability (0.0 - 1.0)
#   cond=<name>         trigger condition (same names as drum conditions)
#
# Chord tags (turns this step into a polyphonic chord):
#   chord=<type>        chord type built from the root note:
#                         single | fifth | triad | inv1 | inv2 | seventh
#                         octave | octaves | custom
#   pick=<mode>         how to select notes from the chord each time it triggers:
#                         cycleUp | cycleDown | pingpong | random | randomWalk | all
#                         (all = play every note simultaneously via chord trigger)
#
# Custom chord voicing (when chord=custom):
#   notes=<n1,n2,...>   exact MIDI notes, comma-separated (up to 8)
#                         Example: notes=60,64,67,72  (Cmaj with octave doubling)

m 0  0 D2  0.35 8
m 0  8 A2  0.30 8
m 1  3 A4  0.25 3
m 1  6 G4  0.20 2
m 1 10 F4  0.22 2
m 1 13 D4  0.18 3
m 2  0 D3  0.20 8  chord=triad pick=cycleUp
m 2  8 A3  0.18 8  chord=triad pick=cycleUp

# ---- parameter locks (Elektron-style per-step automation) ----
# Format: p <track> <step> <param> <value>
#
#   track:  0-6 (absolute: 0-3 = drum tracks, 4-6 = melody tracks bass/lead/chord)
#   step:   0-31
#   param:  one of:
#             cutoff        filter cutoff (0.0 - 1.0) [melody]
#             reso          filter resonance (0.0 - 1.0) [melody]
#             filterEnv     filter envelope amount (-1.0 to 1.0) [melody]
#             decay         amplitude decay (0.0 - 5.0) [all]
#             volume        step volume multiplier (0.0 - 2.0) [all]
#             pitch         pitch offset in semitones (-12.0 to 12.0) [all]
#             pulseWidth    PWM width (0.1 - 0.9) [melody]
#             tone          brightness (0.0 - 1.0) [drums: per-drum tone, melody: alias for cutoff]
#             punch         punch (0.0 - 1.0) [kick: punchPitch depth, snare: snappy amount]
#             nudge         timing offset in ticks (-12 to 12) [all]
#             flamTime      flam ghost note delay in ms (0 - 50) [drums]
#             flamVelocity  flam ghost note velocity multiplier (0.0 - 1.0) [drums]
#   value:  float

p 6  0 cutoff 0.3
p 3 12 nudge  3.0

# ---- automation lanes (parameter changes over time within a pattern) ----
# Format: a <track> <param> <shape> <start> <end> [startStep] [endStep]
#
# Unlike p-locks (per-step snapshots), automation is continuous — it smoothly
# changes a parameter across a range of steps. This is how you do filter sweeps,
# volume fades, gradual pitch bends, etc.
#
#   track:     0-6 (absolute: 0-3 = drum, 4-6 = melody) or "master"
#   param:     same names as p-lock params, plus:
#                masterVolume, reverbMix, delayMix, delayFeedback,
#                fmModIndex, filterLfoDepth, busVolume
#   shape:     interpolation curve:
#                linear       — straight line from start to end
#                easeIn       — slow start, fast end (exponential)
#                easeOut      — fast start, slow end (logarithmic)
#                easeInOut    — S-curve
#                sine         — one cycle of sine (good for sweeps that return)
#                triangle     — up then down (or down then up)
#   start:     parameter value at the beginning
#   end:       parameter value at the end
#   startStep: first step (default 0)
#   endStep:   last step (default = track length - 1, i.e. whole pattern)
#
# Examples:
#   # Bass filter opens over the whole bar (House-style acid sweep)
#   a 4 cutoff linear 0.08 0.65
#
#   # Reverb builds during second half of pattern
#   a master reverbMix easeIn 0.1 0.6 8 15
#
#   # FM index wobble — sine across whole pattern (stab brightness modulation)
#   a 6 fmModIndex sine 0.5 3.0
#
#   # Volume fade out over last 4 steps
#   a 4 volume easeOut 1.0 0.0 12 15
#
#   # Ping-pong filter (triangle: opens then closes)
#   a 4 cutoff triangle 0.1 0.7

# ---- markers (intra-pattern transition/cue points) ----
# Format: marker <step> "<name>"
#
# Marks a step as a valid transition point. The DJ system can say
# "jump to combat at the next marker" — the sequencer waits until
# it reaches a marker step, then transitions (instead of waiting
# for the whole pattern to finish).
#
# Also used for music→game callbacks: when playback reaches a marker,
# a MUSIC_EVENT_MARKER callback fires, letting the game sync visuals
# to specific musical moments.
#
# marker 0  "downbeat"
# marker 4  "bridge_point"
# marker 8  "half"
# marker 12 "pre_fill"

# ============================================================================
# PER-PATTERN INLINE INSTRUMENT OVERRIDES (optional)
# ============================================================================
# If a pattern sets `instrument.bass = inline`, define it here.
# Same format as [instrument.bass] but scoped to this pattern only.
#
# [pattern.0.instrument.bass]
# waveType = saw
# attack = 0.003
# filterCutoff = 0.08
# filterResonance = 0.3

# ============================================================================
# TEMPO CHANGES
# ============================================================================
# Tempo can change per pattern via the `bpm` override (instant change at
# pattern boundary). For gradual tempo changes within a pattern, use
# automation on the special `bpm` target:
#
#   a master bpm linear 120.0 80.0           # ritardando over whole pattern
#   a master bpm easeOut 80.0 120.0 0 7      # accelerando in first half
#
# Note: tempo automation requires the sequencer to recalculate tick duration
# each step. The resolution is per-step (not per-tick), which is fine for
# musical ritardando/accelerando but not for smooth continuous tempo curves.

# ============================================================================
# Additional patterns follow the same format
# ============================================================================

[pattern.1]
drumTrackLength   = 16, 16, 16, 16
melodyTrackLength = 16, 16, 16

d 2  6 0.10
d 2 14 0.12

m 0  0 D2  0.30 16
m 1  4 A4  0.20 4
m 1 12 D4  0.18 4  slide
m 2  0 D3  0.15 16 chord=triad pick=cycleUp
```

### Why text, not binary

- Human-readable — can diff, hand-edit, debug
- AI-friendly — the MIDI→song Python script outputs this directly, and we (Claude) can read/write it
- No versioning headaches — new fields get defaults when absent, old files keep working
- Matches the spirit of how songs are currently authored (readable C code)

Binary would be faster to parse but we have at most 8 patterns × 32 steps × 7 tracks — parsing time is negligible.

### Note name format

MIDI notes can be written as names or numbers:
- `C4` = middle C (MIDI 60), `A4` = 440 Hz (MIDI 69)
- Sharps: `C#4`, `F#3`
- Flats: `Eb2`, `Bb4`
- Numbers: `60`, `69` (raw MIDI note)
- `REST` or `-` = explicit rest/silence
- Octave range: 0-9 (C0 = MIDI 12 through G9 = MIDI 127)

## What Needs to Change

### In the Synth/Sequencer Engines (soundsystem/engines/)

Mostly nothing — the core data structures (Pattern, SynthPatch, DrumParams, Effects, BusEffects) already exist. But the new features need some additions:

**Automation system (new):**
- `AutomationLane` struct: target track, param, shape, start/end values, start/end steps
- Per-pattern array of lanes (e.g., `AutomationLane automation[16]`, `int automationCount`)
- Sequencer tick loop evaluates active lanes each step, interpolates, applies value
- Needs integration point in `seqUpdateDrumTrack` / `seqUpdateMelodyTrack`

**Per-pattern overrides (new):**
- Pattern struct needs optional override fields: scale root/type, BPM, groove, mute mask, per-track instrument index
- These could be a `PatternOverrides` struct with a `hasOverride` bitmask to distinguish "not set" from "set to 0"

**Tempo automation:**
- Sequencer's `tickTimer` calculation currently uses fixed `seq.bpm`. Needs to support per-step BPM from automation lane, recalculating tick duration at step boundaries.

### In the DAW (soundsystem/demo/demo.c)

**Save/Load files:**
- `saveSong(filepath)` — serialize current state (patches + drums + effects + sequencer patterns + mix + arrangement) to `.song`
- `loadSong(filepath)` — parse `.song`, populate all state
- `savePatch(filepath, patchIndex)` — serialize one SynthPatch to `.patch`
- `loadPatch(filepath, patchIndex)` — parse `.patch` into a patch slot
- `saveBank(filepath)` — serialize all 4 patches to `.bank`
- `loadBank(filepath)` — parse `.bank` into patch slots

**Scene struct expansion:**
- Add `Pattern patterns[SEQ_NUM_PATTERNS]` to Scene (currently missing — scenes don't save sequencer data)
- Add `float bpm`, `int patternCount`, `bool use32ndNoteMode`
- Add `float trackVolume[SEQ_TOTAL_TRACKS]`
- Add `int drumTrackSound[SEQ_DRUM_TRACKS]` (which drum type per track)
- Add arrangement: section names, pattern indices, length
- Add scale settings: `bool scaleLockEnabled`, `int scaleRoot`, `int scaleType`
- Add per-pattern overrides (scale, groove, instrument refs, mute mask)
- Add automation lanes per pattern
- Add song metadata (mood, energy, context)
- Add fade in/out settings

**UI additions:**
- File save/load buttons (or key shortcuts) for `.song`
- Patch save/load per slot
- Arrangement editor: row of named sections mapped to patterns, drag to reorder
- Per-pattern override panel: when editing a pattern, show which song-level settings are overridden
- Mute/solo per track per pattern (visual toggle in pattern header)
- Automation lane editor: select param, draw start/end values + curve shape
- Song metadata editor: mood tags, energy, context dropdowns

**Patch naming:**
- Add `char name[32]` to SynthPatch so patches carry their identity

**Missing SynthPatch fields** (exist in synth engine but not yet in demo.c's SynthPatch):
- `bool p_expRelease` — exponential vs linear release (exists on Voice + global `noteExpRelease`)
- These need adding to the struct before the file format can serialize them

### In the Game (src/sound/)

**New: runtime song loader:**
- `SongFile` struct — parsed representation of a `.song` (instruments as SynthPatch, patterns, arrangement, mix, drum config)
- `LoadSongFile(const char* path)` → `SongFile*`
- `FreeSongFile(SongFile*)`
- Generic trigger function that reads from SynthPatch at trigger time (replaces per-song trigger functions)
- `SoundSynthPlaySongFile(SoundSynth*, SongFile*)` — configures synth from SongFile, loads patterns into SongPlayer

**Generic instrument trigger:**

Right now each song has bespoke trigger functions like `melodyTriggerPluckBass()` that set globals and call `playPluck()`. With file-based instruments, we need ONE generic trigger that reads a SynthPatch and dispatches:

```c
static void genericMelodyTrigger(SynthPatch* patch, int track, int note, float vel,
                                  float gateTime, bool slide, bool accent) {
    // Apply patch envelope/filter to synth globals
    noteAttack = patch->p_attack;
    noteDecay = patch->p_decay;
    // ... etc
    noteVolume = vel * patch->p_volume;

    float freq = midiToFreqScaled(note);

    // Dispatch by wave type
    switch (patch->p_waveType) {
        case WAVE_PLUCK:
            v = playPluck(freq, patch->p_pluckBrightness, patch->p_pluckDamping);
            break;
        case WAVE_FM:
            fmModRatio = patch->p_fmModRatio;
            fmModIndex = patch->p_fmModIndex;
            fmFeedback = patch->p_fmFeedback;
            v = playFM(freq);
            break;
        case WAVE_MALLET:
            v = playMallet(freq, patch->p_malletPreset);
            break;
        case WAVE_ADDITIVE:
            v = playAdditive(freq, patch->p_additivePreset);
            break;
        // ... all 13 wave types
    }
}
```

This already effectively exists in demo.c's keyboard trigger path — it applies the current patch and calls the right play function. We just need to extract it as a reusable function.

**SongPlayer changes:**
- Add `SynthPatch instruments[3]` (one per melody track, song-level defaults)
- Add per-pattern instrument overrides (when pattern specifies different instrument)
- Add `int drumSounds[4]` (drum type per track)
- Add arrangement: section names, pattern indices, length, current index
- Pattern cycling follows arrangement order instead of linear 0→N
- Add per-pattern override application: at pattern boundary, check for scale/groove/instrument/mute overrides, apply them (restore song defaults when leaving)
- Add automation lane evaluation: each step, interpolate active lanes and apply values
- Add tempo automation: recalc tick duration when BPM lane is active
- Add fade in/out: volume envelope at song start/stop, controlled by `[transitions]`

**Song selection by the game:**
- `SongFile` carries metadata (mood, energy, context)
- Game queries available songs: "give me a calm, low-energy song for dormitory at night"
- `SoundSynthFindSong(mood, energy, context)` filters loaded songs, picks one (random or weighted)
- Crossfade support: `SoundSynthCrossfadeTo(synth, nextSong)` blends out current, blends in next over `fadeOut`/`fadeIn` durations

**Existing compiled songs:**
- Keep working as-is (they use their own trigger callbacks). No migration needed.
- Optionally convert them to `.song` files over time, gaining metadata + automation for free.

### MIDI Conversion Pipeline

Change Python script output from C code to `.song` text format. Same quantization logic, different output format. The script can also auto-select instruments from a default bank (e.g., piano tracks → "fm rhodes", bass → "pluck bass").

## Phases — Dependency Map & Easy Wins

### What already exists (important — don't rebuild these)

The DAW already has more than the doc previously suggested:

- **`playNoteWithPatch(freq, SynthPatch*)`** (demo.c:1413) — the generic instrument trigger already exists! It calls `applyPatchToGlobals`, then dispatches by wave type to `playPluck/playFM/playMallet/etc.` This is exactly what Phase 2 described as needed. The DAW's sequencer already uses it via `melodyTriggerGeneric`.
- **`melodyTriggerGeneric(trackIdx, patchIdx, freqMult, ...)`** (demo.c:1559) — handles p-lock application, accent, slide/glide, and calls `playNoteWithPatch`. Full featured.
- **`drumTriggerWithPLocks(drumType, vel, pitch)`** (demo.c:1439) — generic drum trigger with p-lock support.
- **`SynthPatch` struct** (demo.c:404) — already covers ~95% of synth parameters. Only missing: `name[32]`, `expRelease`.
- **Scene save/load to RAM** — works, just missing pattern data and file I/O.

What the DAW is **missing** for sequencer playback:
- **No chord trigger callback** — `setMelodyChordCallbacks` is never called, so `PICK_ALL` note pools don't work in the DAW. The game bridge has this (`melodyChordTriggerFMKeys` etc.) but the DAW doesn't.
- **No `melodySustain` UI** — the per-step sustain-hold field exists in the Pattern struct and the sequencer processes it, but the step inspector has no control for it.

### Dependency graph

```
                    ┌─────────────────┐
                    │  1a. File format │ (song_file.h — parser/serializer)
                    │     parser       │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
     ┌────────────┐  ┌────────────┐  ┌────────────┐
     │ 1b. DAW    │  │ 3. Game    │  │ 5. MIDI    │
     │ save/load  │  │ runtime    │  │ pipeline   │
     │ UI         │  │ loader     │  │ update     │
     └────────────┘  └─────┬──────┘  └────────────┘
                           │
                    ┌──────┴──────┐
                    │ 6. Metadata │
                    │ + song pick │
                    └──────┬──────┘
                           │
                    ┌──────┴──────┐
                    │ 7. DJ       │
                    │ system      │
                    └─────────────┘

Independent (can be done anytime, no dependencies):
  1c. Scene struct expansion (add patterns/BPM/scale to Scene)
  1d. SynthPatch additions (name, expRelease)
  1e. DAW chord trigger callback (PICK_ALL support)
  1f. DAW melodySustain UI in step inspector
  2.  Arrangement editor UI
  4.  Automation lanes (engine + UI)
  8.  MIDI keyboard input
  9.  Piano roll view
  10. Song metadata editor UI
```

### Phase 1: Foundation (DAW persistence)

All items here are independent of each other and can be done in any order or in parallel.

**1a. File format parser/serializer** `[blocker — everything else depends on this]` **DONE**
- `soundsystem/engines/song_file.h` (1321 lines) — full `.song` text parser and serializer
- `saveSongFile(filepath, ...)` / `loadSongFile(filepath, ...)` for complete songs
- `savePatchFile(filepath, SynthPatch*)` / `loadPatchFile(filepath, SynthPatch*)`
- 354 lines of tests in `test_soundsystem.c` (round-trip write→read→compare)
- Example file: `soundsystem/demo/songs/scratch.song`

**1b. DAW save/load UI** `[depends on 1a]` **DONE**
- DAW wired to `saveSongFile` / `loadSongFile`
- demo.c refactored (-373/+484 lines) to use shared patch/trigger code

**1c. Scene struct expansion** `[independent — easy win]` **DONE**
- Added to Scene: `Pattern patterns[8]`, `float bpm`, `int ticksPerStep`, `float trackVolume[7]`, `DrumType drumSounds[4]`, `bool sceneScaleLockEnabled`, `int sceneScaleRoot`, `int sceneScaleType`
- Updated `saveScene()` to copy `seq.patterns`, `seq.bpm`, `seq.ticksPerStep`, `seq.trackVolume`, `drumTrackSound`, scale globals
- Updated `loadScene()` to restore them all (including drum track names via `updateDrumTrackName`)
- Scenes are now complete snapshots — patterns, BPM, resolution, volumes, drum sounds, and scale lock all survive scene save/load.

**1d. SynthPatch additions** `[independent — easy win]` **DONE**
- Added `char p_name[32]` and `bool p_expRelease` to SynthPatch
- Updated `createDefaultPatch()` to init both (empty name, false expRelease)
- Updated `applyPatchToGlobals()` to set `noteExpRelease` from patch

**1e. DAW chord trigger** `[independent — easy win]` **DONE**
- Added `melodyChordTriggerGeneric()` — releases previous voices, then plays each chord note via `melodyTriggerGeneric`
- Added per-track wrappers: `melodyChordTriggerBass/Lead/Chord`
- Registered all 3 via `setMelodyChordCallback()` in main init
- PICK_ALL chords now work in the DAW sequencer.

**1f. Melody sustain UI** `[independent — easy win]` **DONE**
- Added "Sus:N" control in melody step inspector (after Accent toggle)
- Click to increment (0-16), right-click to decrement, scroll wheel for fine adjust
- Green highlight when sustain > 0
- Reads/writes `p->melodySustain[track][step]` — engine already processes it.

### Phase 2: Generic trigger in game bridge — **DONE**

`[depends on 1d for expRelease, otherwise independent]`

1. ~~Move `playNoteWithPatch` and `applyPatchToGlobals` from demo.c to shared header~~ → `soundsystem/engines/patch_trigger.h` (141 lines)
2. ~~Extract `SynthPatch` struct from demo.c~~ → `soundsystem/engines/synth_patch.h` (257 lines)
3. ~~`SoundSynthPlaySongFile()` sets up generic triggers instead of per-song ones~~ → implemented in `sound_synth_bridge.c` (+158 lines)
4. Existing compiled songs keep their own callbacks — no migration needed

### Phase 3: Game runtime loader — **DONE**

`[depends on 1a + Phase 2]`

1. ~~Include `song_file.h` in game build~~ → done
2. ~~`LoadSongFile()` → parse `.song` → populate SongPlayer patterns + instruments + arrangement~~ → done
3. ~~`SoundSynthPlaySongFile(synth, songFile)` — configures everything and starts playback~~ → done
4. Round-trip works: compose in DAW → save `.song` → load in game

### Phase 4: Automation

`[independent of Phases 1-3 — can be done in parallel]`

Engine work:
1. `AutomationLane` struct: track, param, shape, startVal, endVal, startStep, endStep
2. Add `AutomationLane automation[16]; int automationCount;` to Pattern struct
3. In sequencer step update: evaluate active lanes, interpolate, apply value to current p-lock state
4. BPM as special target: recalc `tickTimer` period when BPM lane is active

DAW UI:
5. Automation lane strip below sequencer grid
6. Parameter/track selector, curve shape selector, draggable start/end points

Can be developed and tested entirely within the DAW — no game changes needed.

### Phase 5: MIDI pipeline update

`[depends on 1a]`

1. Update Python script: output `.song` format instead of C code
2. Default `.bank` file with common instruments (pluck bass, fm rhodes, etc.)
3. Convert existing 5 MIDI songs
4. Test: MIDI → `.song` → load in DAW → tweak → save → load in game

### Phase 6: Song metadata + game integration

`[depends on Phase 3 for game loader]`

1. Add `[metadata]` and `[transitions]` to parser
2. Game scans `assets/songs/` at startup, loads metadata from each
3. `SoundSynthFindSong(mood, energy, context)` — filter+pick
4. Crossfade engine: two SongPlayers running briefly during transitions
5. Game hooks: mover enters dormitory → find calm song → crossfade

### Phase 7: DJ system + iMUSE features

`[depends on Phase 6 for crossfade, Phase 3 for runtime loader]`

1. `DJState` struct in `sound_synth_bridge.c`
2. DJ API: `DJ_FadeTrack`, `DJ_MuteTrack`, `DJ_JumpToSection`, `DJ_Transpose`, `DJ_PlayStinger`, etc.
3. Beat-aligned transitions (wait for pattern boundary or marker)
4. Music→game callbacks (`DJ_SetCallback` with beat/bar/marker/section events)
5. Transition patterns (bridge pieces between sections)
6. Note sustain across pattern boundaries
7. Stinger overlay playback
8. State serialization (save/restore DJ + SongPlayer state)

### Independent UI work (anytime, no dependencies)

These are all purely DAW UI work. None depend on the file format or game integration. Can be done whenever convenient:

| Task | Effort | Value |
|---|---|---|
| **Arrangement editor** (section row above pattern bar) | Medium | High — can't compose multi-section songs without it |
| **Per-pattern overrides panel** (scale/instrument/mute toggles) | Medium | High — needed for section variation |
| **Song settings panel** (name, resolution, loops per pattern) | Small | Medium |
| **Song metadata editor** (mood/energy/context tags) | Small | Medium — needed for Phase 6 |
| **Patch name editing** | Tiny | Small — quality of life |
| **Piano roll view** | Large | High — alternative note editing |
| ~~**MIDI keyboard input**~~ | ~~Medium~~ | ~~High~~ — **DONE** (`bf4f8d9`) |
| **Recording mode** | Medium | High — capture played notes |
| **Undo/redo** (pattern snapshots) | Medium | Medium — safety net |
| **Tooltips on hover** | Small | Small — discoverability |

### Suggested attack order

**Completed:**
1. ~~1c — Scene struct expansion~~
2. ~~1d — SynthPatch name + expRelease fields~~
3. ~~1e — DAW chord trigger callback~~
4. ~~1f — Melody sustain UI in step inspector~~
5. ~~1a — File format parser/serializer~~
6. ~~1b — DAW save/load UI~~
7. ~~Phase 2 + 3 — Generic trigger + game runtime loader~~
8. ~~MIDI keyboard input~~

**Next up:**
9. Arrangement editor UI (for composing multi-section songs)
10. Phase 4 (automation lanes)
11. Phase 5 (MIDI pipeline outputs `.song` instead of C)
12. Phase 6 (song metadata + context-aware picking + crossfade)
13. Phase 7 (DJ system + iMUSE features)
14. Recording mode, piano roll, undo/redo

---

## DAW UI — What Needs to Be Added

The DAW currently has: 6 collapsible synth/effect columns, a mixer strip with 7 buses + detail panel, a sequencer grid with drum/melody step editors and p-lock inspectors, 8 scene slots (RAM only), pattern bank (1-8), groove controls, rhythm generator, and keyboard input. All real-time, all immediate-mode UI.

Below is everything that's missing to support the full `.song` format and the features described in this doc. Grouped by area.

### 1. File I/O (nothing exists yet)

The DAW has zero file operations. Scenes are RAM-only and lost on quit.

**Needed:**
- **Save Song** button/shortcut (Ctrl+S or similar) → file picker or auto-name → writes `.song`
- **Load Song** button/shortcut (Ctrl+O) → file picker → reads `.song`, populates everything
- **Save Patch** (per slot) → writes `.patch` file for the currently selected patch
- **Load Patch** (per slot) → reads `.patch` file into Bass/Lead/Chord slot
- **Save Bank** → writes all 3 instrument patches (Bass/Lead/Chord) as a `.bank`
- **Load Bank** → reads `.bank`, loads into patch slots
- **Recent files list** (optional but nice) → quick access to last N saved songs
- **New Song** → clears everything to defaults (with confirmation if unsaved)

Where in the UI: top bar, next to Play/Stop and BPM. Small buttons: `[Save] [Load] [New]`. Patch save/load in the Wave column header next to the patch selector.

### 2. Song Settings Panel (nothing exists yet)

Currently there's no concept of "song-level" settings — BPM is in the top bar, scale is buried in the Synth column, and that's it.

**Needed — a `[Song]` collapsible section** (top bar or new panel):
- **Song name** — editable text field
- **BPM** — already exists in top bar, keep it there
- **Resolution mode** — toggle: 16th / 32nd note mode
- **Loops per pattern** — how many times each arrangement entry loops (1-8)
- **Scale lock** — already exists in Synth column. Consider duplicating here since it's song-level, or just leave it. But it should be clear this is a song-level setting, not just a keyboard helper.

### 3. Arrangement Editor (nothing exists yet)

Currently: 8 pattern slots in the pattern bar. Click to queue, shift+click to copy. No concept of song order, no sections, no names.

**Needed — an arrangement row** above or below the pattern bar:

```
[Arrangement]  intro:0  verse:1  verse:1  chorus:2  bridge:3  verse:1  chorus:2  outro:4  [+]
```

- **Section list** — horizontal row of entries, each showing `name:patternIndex`
- **Add section** `[+]` button at the end → appends a new entry
- **Remove section** — right-click an entry → delete it
- **Reorder** — drag entries left/right to rearrange
- **Edit name** — click the name part → inline text edit (or cycle through preset names: intro, verse, chorus, bridge, outro, break, buildup, drop)
- **Edit pattern** — click the number part → cycle through pattern indices (or scroll wheel)
- **Current position indicator** — highlight which section is currently playing
- **Loop indicator** — show the loopsPerPattern count (small number badge)

This replaces the need to mentally track "pattern 0 = intro, pattern 1 = verse" — it's visible and editable.

### 4. Per-Pattern Overrides Panel (nothing exists yet)

Currently: all patterns share the same instruments, scale, groove, and mix. The only per-pattern data is the notes themselves.

**Needed — an overrides section** in the step inspector area (or a collapsible panel that appears when editing a pattern):

- **Scale override** — toggle "override song scale" → root + type selectors (only when overridden)
- **BPM override** — toggle "override song tempo" → BPM slider
- **Groove override** — toggle → swing, jitter, kick/snare/hat/clap nudge sliders
- **Instrument override** per track — toggle "override bass/lead/chord" → shows a mini patch selector (pick from bank or current patch slots)
- **Track mute/solo** — 7 toggle buttons (one per track: K S H C B L Ch) showing mute state for this pattern. Distinct from the mixer mute (which is global/live) — this is "pattern 2 has no drums"
- **Visual indicator** — when a pattern has overrides, show an icon or color change on its pattern bar slot (e.g., orange dot = has overrides)

### 5. Automation Lane Editor (nothing exists yet)

Currently: p-locks give per-step parameter snapshots. No continuous curves.

**Needed — automation lanes** below the sequencer grid (or as a switchable view):

- **Lane selector** — dropdown or tabs: which parameter to automate (cutoff, volume, reverbMix, fmModIndex, bpm, etc.)
- **Track selector** — which track this automation applies to (drum0-3, bass, lead, chord, master)
- **Curve display** — horizontal strip (same width as sequencer grid), showing the interpolated curve from start to end value
- **Start/end value** — draggable handles at left and right edges of the lane
- **Start/end step** — draggable horizontal range (which portion of the pattern the automation spans)
- **Curve shape** — selector: linear, easeIn, easeOut, easeInOut, sine, triangle
- **Multiple lanes** — show stacked, or use tabs to switch between active lanes for this pattern
- **Add/remove lane** — `[+]` to add a new automation lane, right-click to delete
- **Visual feedback** — the affected parameter's value should update in real-time during playback as the automation runs

Simplest first implementation: a single lane per pattern, drawn as a colored line across the step grid, with two draggable endpoints and a shape selector.

### 6. Song Metadata Editor (nothing exists yet)

**Needed — a `[Meta]` panel** (collapsible, in top bar or song settings area):

- **Author** — text field
- **Description** — text field
- **Mood tags** — multi-select from: calm, tense, melancholy, hopeful, playful, mysterious, triumphant, peaceful, urgent, dreamy, warm, dark, upbeat (toggle buttons or checkboxes)
- **Energy** — 3-way selector: low / medium / high
- **Context** — multi-select from: dormitory, workshop, outdoors, exploration, combat, menu, rain, night, celebration, death, any
- **Fade in / fade out** — two sliders (0-10 seconds each)
- **Crossfade** — toggle

This is low-interaction, mostly set-and-forget. Compact layout is fine.

### 7. Mixer Enhancements

The mixer already has 7 bus strips with volume, mute, solo, and per-bus effects (filter, distortion, delay, reverb send). It's actually quite good.

**Missing:**
- **Drum sound selector per bus** — currently you change drum sounds by scrolling on the track label in the sequencer. Would be nice to also show/change in the mixer strip header (click bus name → cycle drum sound)
- **Track volume in mixer vs track volume in sequencer** — these are the same (`trackVolume[]`), just shown in two places. Verify they stay in sync.
- **Master bus effects** — the mixer detail panel only shows per-bus effects. Master effects (reverb, delay, etc.) are in the Effects column. This is fine but could cause confusion — consider showing "Master" as a clickable strip that opens the master effects in the detail panel instead of switching columns.

### 8. Patch Management

Currently: 4 patch slots (Preview, Bass, Lead, Chord), a cycle selector, and a "Load" preset dropdown with 24 built-in presets.

**Missing:**
- **Patch name** — editable name field per patch (currently unnamed, just "Bass"/"Lead"/"Chord" by role)
- **Save patch to file** — button next to patch selector
- **Load patch from file** — button next to patch selector
- **Bank browser** — list of `.bank` files, click to preview/load. Could be a small scrollable list panel.
- **Copy patch between slots** — exists for Preview→Bass/Lead/Chord. Add Bass↔Lead↔Chord copies too.
- **"New patch" / "Init patch"** — reset current slot to default saw

### 9. Melody Step Editor Enhancements

The melody step inspector already handles: note, velocity, gate, slide, accent, probability, condition, p-locks, and note pools (chord type, pick mode, custom notes).

**Missing:**
- **Sustain** — the `melodySustain[track][step]` field exists in the sequencer but has no UI control in the step inspector. Need a "Sustain" slider (0-16 extra steps after gate)
- **Note pool custom notes** — when chord=custom, need a way to enter exact MIDI notes. Currently the note pool section shows chord type and pick mode but no custom note entry. Need: a small list of up to 8 note names, editable by scroll wheel per slot, with add/remove buttons.

### 10. Help / Onboarding

Currently: two help overlays (keyboard shortcuts + sequencer tips) toggled by `[?]` buttons.

**Would be nice:**
- **Tooltips on hover** — show parameter name + current value + range when hovering any slider/control
- **Contextual help** — when a new feature panel (automation, arrangement, metadata) is first opened, show a one-line hint

### 11. Undo/Redo (stretch goal)

Currently: no undo. Every click is permanent (until you load a scene or restart).

For a DAW this is painful. At minimum:
- **Undo last step edit** — single-level undo for note placement/deletion
- Ideal: multi-level undo stack for all operations

This is complex to implement in immediate-mode UI with global state. Could start with just snapshotting the Pattern struct before each edit (cheap — Pattern is a fixed-size struct).

### Summary: Priority Order

| Priority | Feature | Why |
|---|---|---|
| **Must have** | File save/load (.song, .patch) | Without this, nothing persists |
| **Must have** | Song settings (name, resolution, loops) | Basic song identity |
| **Must have** | Arrangement editor | Can't compose multi-section songs without it |
| **Must have** | Melody sustain control | Exists in engine, no UI |
| **Should have** | Per-pattern overrides (scale, instrument, mute) | Needed for sections that differ |
| **Should have** | Automation lane editor | Needed for sweeps/fades |
| **Should have** | Patch save/load + naming | Instrument reuse across songs |
| **Should have** | Song metadata | Game integration needs it |
| **Should have** | MIDI keyboard input | Play notes with real controller |
| **Should have** | Piano roll view | Visual note editing, fix mistakes |
| **Should have** | Recording mode (step capture) | Capture played notes into patterns |
| **Nice to have** | Custom note pool entry | Exact chord voicings |
| **Nice to have** | Mixer drum sound selector | Quality of life |
| **Nice to have** | Undo/redo | Safety net |
| **Nice to have** | Tooltips | Discoverability |

---

## Live DJ System

Separate from the static song format, the game needs a **runtime layer** that can manipulate a playing song in response to what's happening in the world. Think of it as an AI DJ or a dynamic music mixer sitting between the song data and the audio output.

This is NOT saved in the `.song` file — the `.song` defines the composition, the DJ system reacts to it live.

### What the DJ controls

**Track-level mixing (already exists via `trackVolume[]` + `BusEffects`):**
- Mute/unmute individual tracks in real-time (drop the drums when movers sleep)
- Fade track volumes up/down over time (bass fades in as danger approaches)
- Solo a track temporarily (only the melody plays during a cutscene)

**Effects manipulation:**
- Wet/dry on reverb, delay (more reverb in caves, less outdoors)
- Filter sweeps on buses (low-pass everything when underground, open up on surface)
- Sidechain depth (pump harder during intense moments)
- Tape wobble / bitcrush for disorientation or dream sequences

**Song flow control:**
- Jump to a specific named section ("skip to chorus" when action peaks)
- Hold/loop current pattern (don't advance during a tense moment)
- Skip to next pattern (resolve tension faster)
- Change `loopsPerPattern` on the fly (extend a calm section, shorten a busy one)

**Instrument swaps:**
- Hot-swap a track's instrument at runtime (lead switches from glockenspiel to FM organ when night falls)
- Adjust patch parameters live (filter opens on bass as colony grows)

**Tempo nudging:**
- Slight BPM increase during action (+5-10%), decrease during rest
- Stays within a natural range so it doesn't feel jarring

### How the game talks to the DJ

The DJ exposes a simple API. Game systems call it, the DJ applies changes smoothly (never instant cuts — always fade/interpolate over a few beats to stay musical):

```c
// Track mixing
void DJ_FadeTrack(int track, float targetVolume, float durationBeats);
void DJ_MuteTrack(int track, float fadeBeats);
void DJ_UnmuteTrack(int track, float fadeBeats);
void DJ_SoloTrack(int track, float fadeBeats);    // mute all others
void DJ_Unsolo(float fadeBeats);                   // restore all

// Effects
void DJ_SetReverbMix(float target, float durationBeats);
void DJ_SetBusFilter(int bus, float cutoff, float durationBeats);
void DJ_SetSidechainDepth(float depth, float durationBeats);

// Song flow
void DJ_HoldPattern(bool hold);                    // prevent pattern advance
void DJ_JumpToSection(const char* sectionName);    // "chorus", "bridge", etc.
void DJ_SkipToNext(void);                          // advance now
void DJ_SetLoopsPerPattern(int loops);

// Instrument
void DJ_SwapInstrument(int melodyTrack, SynthPatch* newPatch, float crossfadeBeats);
void DJ_TweakParam(int track, int param, float value, float durationBeats);

// Tempo
void DJ_NudgeTempo(float bpmOffset, float durationBeats);  // relative to song BPM
void DJ_ResetTempo(float durationBeats);                    // return to song BPM
```

### Game events → DJ reactions (examples)

| Game Event | DJ Response |
|---|---|
| All movers sleeping | Mute drums, fade bass to 50%, increase reverb |
| Mover enters danger zone | Fade in drums, nudge tempo +5 BPM, tighten reverb |
| Fire breaks out | Jump to tense section, unmute all, push sidechain depth |
| Rain starts | Low-pass filter on all buses, add tape wow |
| Night falls | Swap lead instrument to something warmer, reduce energy |
| Workshop crafting | Hold current upbeat pattern, boost hi-hat volume |
| Mover dies | Solo lead, heavy reverb, slow tempo, fade others out |
| All movers idle | Reduce to bass + chords only, lower tempo |
| Underground cave | Heavy reverb, mute chord, darken bass filter |
| Celebration/feast | Full unmute, tempo +10 BPM, bright filters |

### Implementation approach

The DJ is a thin state machine that sits in `sound_synth_bridge.c`:

```c
typedef struct {
    // Pending transitions (interpolate over N beats)
    struct { float target; float rate; } trackFade[SEQ_TOTAL_TRACKS];
    struct { float target; float rate; } reverbFade;
    struct { float target; float rate; } busFilterFade[NUM_BUSES];
    struct { float target; float rate; } tempoFade;

    // Flow control
    bool holdPattern;
    const char* pendingSection;  // jump at next pattern boundary

    // Instrument crossfade
    int swapTrack;                // -1 = none
    SynthPatch swapPatch;
    float swapProgress;           // 0→1, blends old→new
} DJState;
```

Updated once per sequencer step (not per sample — musical resolution is fine). Each pending transition moves its value toward target at `rate` per beat. When a transition completes, it becomes inactive.

The game doesn't need to know about beats or steps — it just says "fade the drums out over 4 beats" and the DJ handles timing. The `durationBeats` parameter converts to rate internally using current BPM.

### What this is NOT

- Not a separate process or thread — just a struct + update function called from the existing audio callback
- Not a replacement for the song format — the song defines the composition, the DJ modifies playback
- Not AI/procedural composition — it doesn't generate notes. It mixes, mutes, swaps, and tweaks what the song already has
- Not persisted — DJ state is transient, resets when a new song loads

---

## iMUSE Comparison — Do We Cover What LucasArts Did?

iMUSE (Interactive Music Streaming Engine) was created by Michael Land and Peter McConnell at LucasArts in 1991. Used in Monkey Island 2, X-Wing, TIE Fighter, Grim Fandango, and more. Patent US5315057A (expired 2011). It remains the gold standard for interactive game music — most modern systems (Wwise, FMOD) descend from ideas it pioneered.

### Feature-by-Feature Comparison

| iMUSE Feature | Our System | Status |
|---|---|---|
| **Seamless transitions at musical boundaries** | DJ system jumps at pattern boundaries, crossfade engine handles fade overlap | **Covered** — our pattern-boundary jumps are equivalent to iMUSE's beat-aligned transitions |
| **Transition sequences (bridge pieces)** | Not yet. We jump directly from pattern A to pattern B | **Gap** — iMUSE lets composers write short transitional musical phrases that play *between* sections |
| **Vertical mixing (layer/unlayer tracks)** | DJ_MuteTrack / DJ_FadeTrack per track, per-pattern mute masks | **Covered** — equivalent to iMUSE's Part On/Off hooks |
| **Hook/Jump system (embedded branch points)** | Per-pattern overrides + arrangement sections + DJ_JumpToSection | **Partially covered** — see analysis below |
| **Note sustain across transitions** | Not yet. When we switch patterns, held notes get released | **Gap** — iMUSE can jump to a new position while letting already-sounding notes ring out |
| **Real-time transpose** | Scale lock exists, but no runtime transpose by game events | **Gap** — iMUSE can transpose globally or per-channel in response to game state |
| **Per-channel instrument swap** | DJ_SwapInstrument per melody track | **Covered** — iMUSE's Part Program hook equivalent |
| **Per-channel volume control** | trackVolume[7] + DJ_FadeTrack with beat-based duration | **Covered** |
| **Tempo changes** | Per-pattern BPM override + automation lane on BPM + DJ_NudgeTempo | **Covered** — more flexible than iMUSE (which uses a speed multiplier) |
| **Marker/cue system** | Named arrangement sections ("intro", "chorus") | **Partially covered** — see below |
| **Beat-synchronized transitions** | Pattern boundaries align to bars. DJ jumps happen at next bar boundary | **Covered** |
| **Volume fading / crossfading** | DJ_FadeTrack, transitions fadeIn/fadeOut, SoundSynthCrossfadeTo | **Covered** |
| **Multiple simultaneous sequences** | One SongPlayer at a time (two during crossfade) | **Partially covered** — iMUSE runs multiple independent player-sequencer pairs |
| **State serialization (save/load)** | Not yet. Song file saves composition but not runtime playback state | **Gap** |
| **Bidirectional communication (music→game)** | Not yet. Game→music (DJ API) exists, but music can't notify the game | **Gap** |
| **Stinger insertion** | Not yet | **Gap** |
| **Loop points within a pattern** | Not yet — patterns always play start to end | **Partially covered** via arrangement repeats |

### Analysis of Key Gaps

**1. Transition Sequences (Bridge Pieces)**

iMUSE's killer feature: when jumping from "exploring" music to "combat" music, a short 2-4 bar transitional phrase plays to bridge the harmonic/rhythmic gap. The composer writes these bridges for every possible transition pair.

Our system jumps directly between patterns. This works when patterns share the same key/feel, but sounds abrupt when the mood shifts dramatically.

**Proposed solution:** Add an optional `transition` field to arrangement entries:

```
[arrangement]
order = explore:0, transition_to_combat:5, combat:1, transition_to_explore:6, explore:0

# Or more elegantly, inline transition references:
order = explore:0, combat:1
transition.explore_to_combat = 5    # pattern 5 plays between explore→combat
transition.combat_to_explore = 6    # pattern 6 plays between combat→explore
```

When the DJ calls `DJ_JumpToSection("combat")`, the system checks if a transition exists for the current→target pair, plays the transition pattern first, then continues into the target. This keeps transitions musically authored (not algorithmic).

**2. Note Sustain Across Transitions**

iMUSE can reposition the sequencer (jump to a new measure) while allowing currently-sounding notes to continue ringing. This makes jumps inaudible — the music "breathes" across boundaries.

Our system releases all voices when switching patterns. This is fine for percussive instruments but causes audible gaps for pads, strings, sustained chords.

**Proposed solution:** When switching patterns, don't call `melodyRelease` for tracks whose new pattern starts with the same note on step 0. Or: add a `sustain across transition` flag per track. The DJ could set this: `DJ_SetTrackSustain(TRACK_CHORD, true)` — chord pad holds through pattern changes.

**3. Real-Time Transpose**

iMUSE can shift the key of the entire piece (or individual channels) by N semitones at runtime. A scene in a minor key can shift to major when the mood changes, without needing a separate pattern.

Our scale lock constrains notes to a scale, but there's no runtime "shift everything up 3 semitones" API.

**Proposed solution:** Add `DJ_Transpose(int semitones, float durationBeats)` and `DJ_TransposeTrack(int track, int semitones, float durationBeats)`. The sequencer applies the offset to MIDI note values before triggering. Combined with scale lock, this could also shift the scale root: `DJ_SetScaleRoot(7)` — modulate from D dorian to G dorian.

**4. Marker / Cue System**

iMUSE embeds markers at specific beat positions within a MIDI sequence. When playback reaches a marker, queued commands fire. This allows the game to say "at the next marker, start the combat music" without knowing exactly when that marker will arrive.

Our named arrangement sections serve a similar purpose at the macro level (section boundaries). But we lack *intra-pattern* markers — points within a pattern where transitions could happen mid-bar.

**Proposed solution:** Add optional markers within patterns:

```
[pattern.0]
marker 4 "bridge_point"    # at step 4, this is a valid transition point
marker 12 "loop_back"      # at step 12, another valid point
```

The DJ can then say `DJ_JumpToSectionAtMarker("combat", "bridge_point")` — wait until the next occurrence of "bridge_point" in the current pattern, then transition. This gives sub-bar precision for transitions without requiring them to happen at pattern boundaries.

**5. State Serialization**

iMUSE can save and restore its complete runtime state (which sounds are playing, at what position, with what hook values, what fades are in progress). Essential for game save/load.

Our SongPlayer state is runtime-only. If the player saves the game, the music restarts from the beginning on load.

**Proposed solution:** Add `SongPlayerSerialize(SongPlayer*, buffer)` / `SongPlayerDeserialize(SongPlayer*, buffer)`. Save: current arrangement index, pattern index, step position, active voices, DJ state (pending fades, held patterns, transpose offset). Integrate with the game's existing save/load system (save version bump).

**6. Bidirectional Communication (Music→Game)**

iMUSE notifies the game when musical events occur (marker reached, transition complete, pattern boundary). This lets the game synchronize visual effects to music.

Our system is one-way: game→DJ. The game can't know when a beat lands or when a transition completes.

**Proposed solution:** Callback system:

```c
typedef void (*MusicEventFunc)(int eventType, int data);
void DJ_SetCallback(MusicEventFunc cb);

// Event types:
// MUSIC_EVENT_BEAT        — every beat (data = beat number within pattern)
// MUSIC_EVENT_BAR         — every bar/pattern start (data = pattern index)
// MUSIC_EVENT_MARKER      — marker reached (data = marker ID)
// MUSIC_EVENT_SECTION     — arrangement section changed (data = section index)
// MUSIC_EVENT_TRANSITION  — transition complete
```

The game could use this to flash lights on beat, trigger particle effects on the downbeat, sync animations to music tempo, etc.

**7. Stinger Insertion**

X-Wing and TIE Fighter use iMUSE to insert brief musical phrases ("stingers") when events occur — a TIE Fighter appears, a fanfare plays over the existing music, then the base music continues. The stinger doesn't interrupt the underlying sequence; it layers on top or briefly takes over a channel.

We have no stinger concept.

**Proposed solution:** Stingers are short one-shot patterns (1-4 steps) that play over the current song without advancing the arrangement. Add:

```c
void DJ_PlayStinger(Pattern* stinger, float volume);  // layer on top, one-shot
void DJ_PlayStingerExclusive(Pattern* stinger);        // briefly take over, then resume
```

In the `.song` file, stingers could be special patterns marked as non-arrangement:

```
[stinger.victory]
m 1 0 C5 0.9 2
m 1 2 E5 0.9 2
m 1 4 G5 0.9 4

[stinger.danger]
d 0 0 0.95
d 1 2 0.80
m 0 0 C2 0.7 4
```

### What iMUSE Does That We Don't Need

- **MIDI hardware management** — iMUSE managed AdLib/Roland sound cards. We synthesize everything in software. Not relevant.
- **Part priority / voice stealing arbitration** — iMUSE needed this for hardware with limited polyphony (8-16 voices). Our synth has 16 voices with software voice stealing already handled.
- **7-bit SysEx encoding** — MIDI protocol limitation. Our format is text-based. Not relevant.
- **Interrupt-driven timing at 144 Hz** — we run per-sample at 44100 Hz with 96 PPQ sequencer. Better precision.

### What iMUSE Does That Modern Systems Struggle With

iMUSE's deepest advantage was operating at the **note level** — it could transpose, sustain, and manipulate individual notes because everything was MIDI. Modern adaptive music systems (Wwise, FMOD) work with pre-recorded audio and can only crossfade, duck, or switch between stems.

**We have this advantage too.** Our synth generates everything in real-time from note data. We can transpose, change instruments, sustain notes across boundaries, and manipulate any parameter at any time — just like iMUSE. We just need to build the runtime infrastructure (DJ system + the gaps listed above).

### Summary: Gaps to Close

| Gap | Difficulty | Phase |
|---|---|---|
| Transition sequences (bridge patterns) | Low — arrangement format change + DJ logic | Phase 7 (DJ) |
| Note sustain across pattern boundaries | Medium — voice management at transition | Phase 7 (DJ) |
| Real-time transpose (global + per-track) | Low — offset before midiToFreqScaled | Phase 7 (DJ) |
| Intra-pattern markers | Low — marker array per pattern + DJ query | Phase 7 (DJ) |
| State serialization | Medium — serialize SongPlayer + DJ state | Phase 6 (game integration) |
| Music→game callbacks | Low — callback function pointer + event enum | Phase 7 (DJ) |
| Stinger insertion | Medium — one-shot pattern overlay without disrupting arrangement | Phase 7 (DJ) |

All gaps land in Phase 6-7. The core file format (Phases 1-5) is solid. The iMUSE-equivalent features are runtime behaviors, not file format issues — they belong in the DJ system and SongPlayer.

---

## MIDI Controller Input + Piano Roll + Recording

Separate from the file format, but critical for the DAW as a composition tool.

### MIDI Keyboard Input (DONE)

Implemented in `soundsystem/engines/midi_input.h` using CoreMIDI (macOS), with no-op stubs for other platforms. Gracefully no-ops when no device is connected.

**What exists:**
- **CoreMIDI integration** — auto-detects first available MIDI source at startup
- **Hot-plug** — plug/unplug devices at any time via `MIDINotification` callback
- **Note on/off** — plays through currently selected patch with MIDI velocity
- **Mod wheel (CC1)** → filter cutoff (updates live voices in real-time)
- **Sustain pedal (CC64)** → holds all notes until pedal released
- **MIDI learn** — right-click any `DraggableFloat` in the UI → wiggle a hardware knob → mapped. Right-click again to unmap. Up to 64 simultaneous CC mappings. Learned CCs update live MIDI voices (filter, vibrato, PWM, volume).
- **Visual indicators** — magenta "MIDI?" when waiting for CC, cyan `[CC##]` when mapped
- **Lock-free ring buffer** — CoreMIDI callback thread → main thread, no mutex needed

**Still TODO:**
- **Pitch bend** — `MIDI_PITCH_BEND` events are parsed but not yet applied to voice frequency
- **MIDI channel routing** — different channels → different patch slots (ch1=bass, ch2=lead, ch3=chord)
- **Device selector** — currently auto-connects to first source. Multi-device selection not yet implemented
- **Expression (CC11)** → volume mapping

### Split Keyboard

Play two instruments on one MIDI keyboard — e.g., bass on the left hand, lead on the right. Two people can jam together on a single keyboard.

**What's needed:**
- **Split point** — a MIDI note number (e.g., C4 = 60) that divides left zone from right zone
- **Zone → patch mapping** — left zone plays one patch (e.g., Bass), right zone plays another (e.g., Lead)
- **Separate voice tracking** — each zone tracks its own voices independently (sustain pedal could be per-zone or global)
- **Octave offset per zone** — left zone might want to be transposed down an octave so both players have comfortable range
- **UI**: toggle split on/off, draggable split point (show note name), left/right patch selector, optional octave offset per zone
- **Scale lock per zone** — optionally different scale constraints per zone (or shared)

**Complexity:** Low-medium (~40-60 lines logic + small UI row). The MIDI input infrastructure already exists — this is just routing notes to different patches based on note number.

**Design sketch:**
```
[Split: ON]  Left: Bass (C2-B3)  |  Split: C4  |  Right: Lead (C4-C7)  [-1 oct] [+0 oct]
```

In the MIDI event handler, replace `selectedPatch` lookup with:
```c
if (splitEnabled) {
    patch = (note < splitPoint) ? &patches[splitLeftPatch] : &patches[splitRightPatch];
    voiceArray = (note < splitPoint) ? splitLeftVoices : splitRightVoices;
    note += (note < splitPoint) ? splitLeftOctave * 12 : splitRightOctave * 12;
}
```

### Recording Mode

With MIDI input, recording becomes possible: play notes on the keyboard while the sequencer runs, capture them into the current pattern.

**What's needed:**
- **Record toggle** (button or R key) — arms recording
- **Quantize on input** — as notes arrive, snap to nearest step (with configurable quantize strength: 100% = grid-locked, 50% = half nudge, 0% = free placement as nudge offset)
- **Overdub vs replace** — overdub adds notes to existing pattern, replace clears the track first
- **Record into which track** — follows the currently selected patch (Bass/Lead/Chord)
- **Post-record quantize** — option to re-quantize after recording (fix timing)
- **Velocity recording** — capture MIDI velocity directly into step velocity
- **Gate from note duration** — note-off minus note-on → gate length in steps

### Piano Roll View

An alternative to the step grid for melody editing. Shows notes as horizontal bars on a pitch-vs-time grid. Standard in every DAW, essential for fixing mistakes and visualizing melodies.

**What's needed:**
- **Switchable view** — toggle between step grid and piano roll (Tab or button)
- **Vertical axis** — MIDI note numbers, with piano key labels on the left (C2, D2, ... C5)
- **Horizontal axis** — steps (same as sequencer grid, 16 or 32 columns)
- **Note bars** — colored rectangles, width = gate length, vertical position = pitch, brightness = velocity
- **Editing:**
  - Click to add note (at step + pitch)
  - Drag note horizontally to change step position
  - Drag note vertically to change pitch
  - Drag right edge to change gate length
  - Right-click to delete
  - Scroll wheel on note to change velocity
- **Track selector** — show one melody track at a time (Bass/Lead/Chord tabs)
- **Slide/accent/sustain indicators** — small icons on note bars (cyan dot for slide, red dot for accent)
- **Note pool visualization** — chord notes shown as stacked semi-transparent bars
- **Playback cursor** — vertical line showing current step during playback

The piano roll is especially useful for:
- Seeing the overall melodic shape at a glance
- Fixing wrong notes from recording or MIDI import
- Editing gate lengths visually (drag the right edge)
- Working with dense chord voicings (custom note pools)

### Where This Fits in the DAW Layout

The piano roll and step grid share the same screen space (bottom section). A toggle switches between them. The step inspector still works in piano roll mode (click a note bar to select it, inspector shows details).

Recording controls go in the transport bar: `[Record] [Overdub/Replace] [Quantize: 100%]`

MIDI device selector goes in a settings panel or as a small dropdown in the transport bar.

---

## References

- **iMUSE Technical Specification** (Nick Porcino / LabMidi): https://raw.githubusercontent.com/meshula/LabMidi/main/LabMuse/imuse-technical.md
  — Definitive technical reference for iMUSE internals: SysEx commands, hook system, marker system, state management, player-sequencer architecture. Extremely valuable for understanding how interactive music systems work at the implementation level.

- **ScummVM iMuse Data Technical Reference**: https://wiki.scummvm.org/index.php/SCUMM/Technical_Reference/iMuse_data

- **The Force Engine: iMuse reimplementation notes**: https://theforceengine.github.io/2022/05/17/iMuseAndSoundRelease.html

- **MIDI-to-Sequencer conversion notes** (our own): `docs/doing/midi-to-sequencer.md`

---

## Open Questions

1. **Where do `.song` files live?** Probably `assets/songs/` in the game, loaded at startup or on demand.
2. **Keep compiled songs?** Yes, they keep working. File-based songs are an addition, not a replacement. Compiled songs are guaranteed zero-load-time.
3. **Bank file location** — `assets/sounds/default.bank`? Or embedded in the binary? Start with files, embed later if needed.
4. **DJ trigger authoring** — who writes the "fire → tense section" mappings? Hardcoded in C for now, eventually a config file or in-game editor?
5. **Multiple simultaneous songs?** — Could the DJ crossfade between two different `.song` files (e.g., outdoor→cave transition plays both songs briefly)? This would need two SongPlayer instances running concurrently.
6. **Section skip behavior** — When the DJ jumps to a named section, does it wait for the current pattern to finish (musical) or cut immediately (responsive)? Probably: jump at next bar boundary by default, with an `immediate` flag for urgent transitions.
7. **Hot-reload for development** — when tweaking a `.song` in the DAW, save, and the game is running: can the game detect the file changed and reload live? Would be a killer workflow (edit in DAW → hear in game instantly). Bigger feature — needs file watching, safe mid-playback reload, state preservation across reload. Worth exploring but not blocking.
8. **Jukebox unification** — the game currently has 13 compiled songs in the jukebox. When file-based songs arrive, the jukebox needs to list both. Could be: scan `assets/songs/` → add to jukebox list alongside compiled entries, or eventually migrate all compiled songs to `.song` files.
9. **MIDI input library** — RtMidi vs PortMidi vs raw CoreMIDI? RtMidi is most portable but C++. PortMidi is C but less maintained. Could start with CoreMIDI-only (macOS) since that's the dev platform.
8. **iMUSE-style hook embedding** — should hooks (branch points) be in the `.song` file itself (like iMUSE embeds them in MIDI), or purely runtime via the DJ API? File-embedded hooks are more self-contained (the song carries its own interactive logic). DJ-only hooks keep songs simple but require game code for every transition. Probably: support both — hooks in the file for common transitions, DJ API for game-specific reactions.
