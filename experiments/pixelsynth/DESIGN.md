# PixelSynth Design Document

## Vision
A groovebox-style music workstation with:
- Multiple sound engines (drums, synth, voice)
- Step sequencer for pattern creation
- Effects chain
- Eventually: pattern chaining, song mode

## Architecture (WIP)

### Tracks
8 tracks, each with:
- Sound engine (drum/synth/voice/sampler)
- Engine params
- Volume / Pan
- Effect sends
- Pattern (16/32/64 steps)
- Per-step parameter locks (future)

### Sound Engines
- **Drum**: 808-style synthesis (kick, snare, hihat, etc)
- **Synth**: Oscillator + filter + envelope
- **Voice**: Formant synthesis
- **Sampler**: SCW wavetables (future: user samples)

### Sequencer
- Step grid (16 steps default, expandable)
- BPM / Swing
- Groove templates (straight, Dilla, etc)
- Live record vs step edit (future)

### Effects
- Per-track sends to shared effects
- Send FX: Delay, Reverb (future)
- Master insert FX: Distortion, Tape, Bitcrusher

### Mixer
- Per-track volume/pan
- Master volume
- Visual meters (future)

## Current State
- [x] 808 drum synthesis
- [x] Synth voices
- [x] Voice/formant synthesis
- [x] Global effects (distortion, delay, tape, bitcrusher)
- [x] Generative music system (hidden, for reference)
- [ ] Drum step sequencer grid
- [ ] Pattern playback
- [ ] Multiple patterns
- [ ] Pattern chaining
- [ ] Per-track architecture

## Next Steps
1. Simple drum grid UI (16 steps x N drums)
2. Pattern playback with loop
3. BPM control
4. Then expand to synth tracks

## Inspiration
- Elektron Digitakt/Syntakt (parameter locks, workflow)
- Roland TR-808/909 (classic drum machine)
- MPC/Maschine (pads + patterns)
- OP-1 (simplicity, tape metaphor)

## Questions
- Fixed track count or dynamic?
- Pattern length: 16/32/64 or fully variable?
- Save/load format?
- MIDI support?
