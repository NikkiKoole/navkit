#!/usr/bin/env python3
"""
midi_variations.py — Analyze chord progressions from MIDI and generate variations.

Takes a MIDI file with chords, extracts the harmonic DNA (key, voicing style,
rhythm, extensions), and generates new patterns in the same style:
  - Re-voicings (drop-2, close, spread, shell)
  - Chord substitutions (relative, tritone, modal interchange)
  - Rhythmic variations (anticipate, delay, syncopate)
  - Extension swaps (add/remove 9ths, 11ths, 13ths)
  - B-section generation (related key areas)

Usage:
    python3 midi_variations.py input.mid                    # writes input-variations.song
    python3 midi_variations.py input.mid -o output.song
    python3 midi_variations.py input.mid --analyze          # analysis only
    python3 midi_variations.py input.mid --variations 8     # generate 8 pattern variations
    python3 midi_variations.py input.mid --seed 42          # reproducible output

Requires: pip install mido
"""

import sys
import os
import argparse
import random
import math
from collections import defaultdict
from dataclasses import dataclass, field

try:
    import mido
except ImportError:
    print("Requires mido: pip install mido", file=sys.stderr)
    sys.exit(1)

# Add tools dir to path for midi_import shared code
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from midi_import import (write_patch_section, load_presets, get_preset_lines,
                         NOTE_NAMES, note_name)

# ---------------------------------------------------------------------------
# Music theory constants
# ---------------------------------------------------------------------------

# Scale intervals (semitones from root)
SCALES = {
    'major':       [0, 2, 4, 5, 7, 9, 11],
    'minor':       [0, 2, 3, 5, 7, 8, 10],
    'dorian':      [0, 2, 3, 5, 7, 9, 10],
    'mixolydian':  [0, 2, 4, 5, 7, 9, 10],
    'lydian':      [0, 2, 4, 6, 7, 9, 11],
    'phrygian':    [0, 1, 3, 5, 7, 8, 10],
    'harmonic_minor': [0, 2, 3, 5, 7, 8, 11],
    'melodic_minor':  [0, 2, 3, 5, 7, 9, 11],
}

# Chord templates: name → intervals from root
CHORD_TEMPLATES = {
    'maj':     (0, 4, 7),
    'min':     (0, 3, 7),
    'dim':     (0, 3, 6),
    'aug':     (0, 4, 8),
    'sus2':    (0, 2, 7),
    'sus4':    (0, 5, 7),
    'maj7':    (0, 4, 7, 11),
    'min7':    (0, 3, 7, 10),
    '7':       (0, 4, 7, 10),
    'min7b5':  (0, 3, 6, 10),
    'dim7':    (0, 3, 6, 9),
    'mMaj7':   (0, 3, 7, 11),
    'maj9':    (0, 4, 7, 11, 14),
    'min9':    (0, 3, 7, 10, 14),
    '9':       (0, 4, 7, 10, 14),
    'min11':   (0, 3, 7, 10, 14, 17),
    'maj13':   (0, 4, 7, 11, 14, 21),
    'min13':   (0, 3, 7, 10, 14, 21),
}

# Chord substitution rules: quality → list of (replacement_quality, root_interval) options
# root_interval is semitones to shift the root
SUBSTITUTIONS = {
    'maj7':  [('min7', 9), ('maj7', 5), ('7', 7), ('min9', 9)],     # vi, IV, V, vi9
    'min7':  [('maj7', 3), ('min7', 5), ('7', 10), ('min7b5', 2)],  # III, iv, bVII, ii°
    '7':     [('7', 6), ('min7', 2), ('dim7', 0), ('min7', 9)],     # tritone, ii, dim, vi
    'min9':  [('maj7', 3), ('min7', 5), ('9', 10)],
    'maj':   [('min', 9), ('maj', 5), ('sus2', 0)],
    'min':   [('maj', 3), ('min', 5), ('sus4', 0)],
}

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class ChordEvent:
    """A detected chord from MIDI."""
    beat: float             # absolute beat position
    pitches: list           # MIDI note numbers, sorted low→high
    velocities: list        # per-note velocity
    duration: float         # beats
    root: int               # pitch class (0-11)
    quality: str            # chord template name
    extensions: list        # extra intervals beyond the template

    @property
    def bar(self):
        return int(self.beat / 4) + 1

    @property
    def beat_in_bar(self):
        return (self.beat % 4) + 1

    @property
    def spread(self):
        return self.pitches[-1] - self.pitches[0] if len(self.pitches) > 1 else 0

    @property
    def name(self):
        ext = ''
        if 14 in self.extensions or 2 in self.extensions: ext += '9'
        if 17 in self.extensions or 5 in self.extensions: ext += '(11)'
        if 21 in self.extensions or 9 in self.extensions: ext += '(13)'
        return f"{NOTE_NAMES[self.root]}{self.quality}{ext}"

@dataclass
class StyleProfile:
    """Extracted style characteristics from a chord progression."""
    key_root: int           # pitch class
    key_quality: str        # 'major', 'minor', 'dorian', etc.
    scale: list             # scale intervals
    avg_chord_size: float
    avg_duration: float     # beats
    avg_spread: float       # semitones
    avg_velocity: float     # 0-127
    vel_range: tuple        # (min, max)
    harmonic_rhythm: str    # 'slow', 'medium', 'fast'
    voicing_style: str      # 'wide', 'close', 'mixed'
    chords: list            # original ChordEvent list
    progression_roots: list # root pitch classes in order

# ---------------------------------------------------------------------------
# MIDI analysis
# ---------------------------------------------------------------------------

def parse_midi_chords(filepath):
    """Parse MIDI file and extract chord events."""
    mid = mido.MidiFile(filepath)
    tpb = mid.ticks_per_beat

    # Extract tempo
    bpm = 120.0
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                bpm = mido.tempo2bpm(msg.tempo)
                break

    # Collect all notes with durations
    notes = []
    for track in mid.tracks:
        tick = 0
        pending = {}  # note → (tick, vel)
        for msg in track:
            tick += msg.time
            if msg.type == 'note_on' and msg.velocity > 0:
                pending[msg.note] = (tick, msg.velocity)
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in pending:
                    start, vel = pending.pop(msg.note)
                    notes.append({
                        'start': start / tpb,
                        'dur': (tick - start) / tpb,
                        'note': msg.note,
                        'vel': vel,
                    })

    notes.sort(key=lambda n: n['start'])

    # Group into chords (notes starting within 0.25 beats)
    groups = []
    cur = []
    for n in notes:
        if not cur or abs(n['start'] - cur[0]['start']) < 0.25:
            cur.append(n)
        else:
            groups.append(cur)
            cur = [n]
    if cur:
        groups.append(cur)

    # Detect chord quality for each group
    chords = []
    for group in groups:
        pitches = sorted([n['note'] for n in group])
        vels = [n['vel'] for n in sorted(group, key=lambda x: x['note'])]
        beat = group[0]['start']
        dur = max(n.get('dur', 1.0) for n in group)
        root, quality, extensions = detect_chord(pitches)
        chords.append(ChordEvent(
            beat=beat, pitches=pitches, velocities=vels,
            duration=dur, root=root, quality=quality, extensions=extensions
        ))

    return bpm, chords


def detect_chord(pitches):
    """Detect chord root, quality, and extensions from MIDI pitches."""
    if len(pitches) < 2:
        return pitches[0] % 12, 'note', []

    pcs = sorted(set(p % 12 for p in pitches))
    bass = min(pitches) % 12

    best_root = bass
    best_quality = '?'
    best_score = -1
    best_ext = []

    for root in pcs:
        ivs = set((pc - root) % 12 for pc in pcs)

        # Try each template, largest first (more specific = better)
        for name, template in sorted(CHORD_TEMPLATES.items(),
                                      key=lambda x: len(x[1]), reverse=True):
            template_ivs = set(template)
            if template_ivs.issubset(ivs):
                score = len(template) * 3
                # Bonus for bass = root
                if root == bass:
                    score += 5
                # Bonus for common chord types
                if name in ('maj7', 'min7', '7', 'min9', 'maj9'):
                    score += 2
                if score > best_score:
                    best_score = score
                    best_root = root
                    best_quality = name
                    best_ext = sorted(ivs - template_ivs)

    return best_root, best_quality, best_ext


def detect_key(chords):
    """Detect most likely key from chord progression."""
    if not chords:
        return 0, 'major', SCALES['major']

    # Count pitch class usage weighted by duration
    pc_weight = [0.0] * 12
    for c in chords:
        for p in c.pitches:
            pc_weight[p % 12] += c.duration

    # Try each root + scale, score by how well pitch classes fit
    best_root = 0
    best_scale_name = 'major'
    best_score = -1

    for root in range(12):
        for scale_name, scale in SCALES.items():
            scale_pcs = set((root + iv) % 12 for iv in scale)
            score = sum(pc_weight[pc] for pc in scale_pcs)
            # Bonus for root being the most common chord root
            root_count = sum(1 for c in chords if c.root == root)
            score += root_count * 2
            if score > best_score:
                best_score = score
                best_root = root
                best_scale_name = scale_name

    return best_root, best_scale_name, SCALES[best_scale_name]


def analyze_style(bpm, chords):
    """Extract style profile from chord progression."""
    key_root, key_quality, scale = detect_key(chords)

    durations = [c.duration for c in chords]
    spreads = [c.spread for c in chords]
    vels = [sum(c.velocities) / len(c.velocities) for c in chords]
    sizes = [len(c.pitches) for c in chords]

    avg_dur = sum(durations) / len(durations) if durations else 4.0
    avg_spread = sum(spreads) / len(spreads) if spreads else 12
    avg_vel = sum(vels) / len(vels) if vels else 64

    if avg_dur >= 4.0:
        harmonic_rhythm = 'slow'
    elif avg_dur >= 2.0:
        harmonic_rhythm = 'medium'
    else:
        harmonic_rhythm = 'fast'

    if avg_spread >= 18:
        voicing_style = 'wide'
    elif avg_spread >= 10:
        voicing_style = 'mixed'
    else:
        voicing_style = 'close'

    return StyleProfile(
        key_root=key_root, key_quality=key_quality, scale=scale,
        avg_chord_size=sum(sizes) / len(sizes) if sizes else 4,
        avg_duration=avg_dur, avg_spread=avg_spread,
        avg_velocity=avg_vel,
        vel_range=(min(vels), max(vels)) if vels else (64, 64),
        harmonic_rhythm=harmonic_rhythm, voicing_style=voicing_style,
        chords=chords,
        progression_roots=[c.root for c in chords],
    )

# ---------------------------------------------------------------------------
# Voicing generators
# ---------------------------------------------------------------------------

def voice_chord(root_pc, intervals, bass_oct, style='wide', target_spread=17,
                num_notes=5, rng=None):
    """Generate a voiced chord as MIDI note numbers.

    Args:
        root_pc: pitch class 0-11
        intervals: tuple of semitone intervals (e.g. (0,4,7,11))
        bass_oct: MIDI octave for the bass note (2-3 typical)
        style: 'wide', 'close', 'drop2', 'shell'
        target_spread: approximate voicing width in semitones
        num_notes: target number of notes
        rng: random.Random instance
    """
    if rng is None:
        rng = random.Random()

    bass_note = root_pc + bass_oct * 12

    if style == 'shell':
        # Root + 3rd/4th + 7th only
        shell = [0]
        if 3 in intervals or 4 in intervals:
            shell.append(3 if 3 in intervals else 4)
        if 10 in intervals or 11 in intervals:
            shell.append(10 if 10 in intervals else 11)
        return sorted([bass_note + iv for iv in shell])

    if style == 'close':
        # Stack intervals within one octave above bass
        voiced = [bass_note]
        for iv in intervals[1:num_notes]:
            voiced.append(bass_note + iv)
        return sorted(voiced)

    if style == 'drop2':
        # Close voicing then drop 2nd-from-top down an octave
        close = sorted([bass_note + iv for iv in intervals[:num_notes]])
        if len(close) >= 3:
            # Drop the second highest note
            second_highest = close[-2]
            close[-2] = second_highest - 12
        return sorted(close)

    # 'wide' — spread across octaves to hit target_spread
    voiced = [bass_note]
    available = list(intervals[1:])  # skip root (already placed)
    # Add extensions if we need more notes
    possible_ext = [14, 17, 21, 9, 2, 5]  # 9, 11, 13 (both octave positions)
    for ext in possible_ext:
        if ext not in intervals and len(available) + 1 < num_notes:
            available.append(ext)

    # Distribute notes across octave range
    target_top = bass_note + target_spread
    for i, iv in enumerate(available[:num_notes - 1]):
        # Place each note, spreading upward
        octave_offset = 0
        if target_spread > 12:
            # For wide voicings, vary octave placement
            progress = (i + 1) / max(len(available[:num_notes - 1]), 1)
            target_note = bass_note + progress * target_spread
            base = bass_note + (iv % 12)
            while base + octave_offset * 12 < target_note - 6:
                octave_offset += 1
        note = bass_note + iv + octave_offset * 12
        # Clamp to reasonable range
        while note > 96:  # C7
            note -= 12
        while note < bass_note:
            note += 12
        voiced.append(note)

    return sorted(set(voiced))  # dedupe


def revoice_chord(original_pitches, root_pc, quality, style, rng=None):
    """Re-voice an existing chord in a different style."""
    if rng is None:
        rng = random.Random()

    template = CHORD_TEMPLATES.get(quality, (0, 4, 7))
    bass_oct = min(original_pitches) // 12
    spread = original_pitches[-1] - original_pitches[0] if len(original_pitches) > 1 else 12
    num_notes = len(original_pitches)

    return voice_chord(root_pc, template, bass_oct, style=style,
                       target_spread=spread, num_notes=num_notes, rng=rng)

# ---------------------------------------------------------------------------
# Variation generators
# ---------------------------------------------------------------------------

def generate_revoicing(chords, style_profile, voicing_style, rng):
    """Same progression, different voicing style."""
    result = []
    for c in chords:
        new_pitches = revoice_chord(c.pitches, c.root, c.quality,
                                     voicing_style, rng=rng)
        # Match velocity style
        vel_center = style_profile.avg_velocity
        vel_range = style_profile.vel_range[1] - style_profile.vel_range[0]
        vels = [max(20, min(120, int(vel_center + rng.uniform(-vel_range/2, vel_range/2))))
                for _ in new_pitches]

        result.append(ChordEvent(
            beat=c.beat, pitches=new_pitches, velocities=vels,
            duration=c.duration, root=c.root, quality=c.quality,
            extensions=c.extensions,
        ))
    return result


def generate_substitution(chords, style_profile, rng):
    """Replace some chords with substitutions."""
    result = []
    for c in chords:
        # 40% chance to substitute
        if rng.random() < 0.4 and c.quality in SUBSTITUTIONS:
            subs = SUBSTITUTIONS[c.quality]
            new_quality, root_shift = rng.choice(subs)
            new_root = (c.root + root_shift) % 12
            template = CHORD_TEMPLATES.get(new_quality, (0, 4, 7))

            bass_oct = min(c.pitches) // 12
            new_pitches = voice_chord(new_root, template, bass_oct,
                                       style=style_profile.voicing_style,
                                       target_spread=int(style_profile.avg_spread),
                                       num_notes=len(c.pitches), rng=rng)
            vels = [max(20, min(120, v + rng.randint(-8, 8))) for v in c.velocities[:len(new_pitches)]]
            while len(vels) < len(new_pitches):
                vels.append(int(style_profile.avg_velocity))

            result.append(ChordEvent(
                beat=c.beat, pitches=new_pitches, velocities=vels,
                duration=c.duration, root=new_root, quality=new_quality,
                extensions=[],
            ))
        else:
            result.append(ChordEvent(
                beat=c.beat, pitches=list(c.pitches), velocities=list(c.velocities),
                duration=c.duration, root=c.root, quality=c.quality,
                extensions=list(c.extensions),
            ))
    return result


def generate_rhythmic_variation(chords, style_profile, rng):
    """Shift chord timing — anticipate, delay, or split."""
    result = []
    for c in chords:
        beat = c.beat
        dur = c.duration
        r = rng.random()
        if r < 0.25:
            # Anticipate by half a beat
            beat = max(0, beat - 0.5)
            dur += 0.5
        elif r < 0.5:
            # Delay by half a beat
            beat += 0.5
            dur = max(0.5, dur - 0.5)
        elif r < 0.65:
            # Split into two hits (repeat chord after half duration)
            half = max(1.0, dur / 2)
            result.append(ChordEvent(
                beat=beat, pitches=list(c.pitches), velocities=list(c.velocities),
                duration=half, root=c.root, quality=c.quality,
                extensions=list(c.extensions),
            ))
            # Second hit slightly softer
            vels2 = [max(20, int(v * 0.75)) for v in c.velocities]
            result.append(ChordEvent(
                beat=beat + half, pitches=list(c.pitches), velocities=vels2,
                duration=dur - half, root=c.root, quality=c.quality,
                extensions=list(c.extensions),
            ))
            continue
        # else: keep original timing

        result.append(ChordEvent(
            beat=beat, pitches=list(c.pitches), velocities=list(c.velocities),
            duration=dur, root=c.root, quality=c.quality,
            extensions=list(c.extensions),
        ))
    return result


def generate_extension_swap(chords, style_profile, rng):
    """Add or remove chord extensions (9ths, 11ths, 13ths)."""
    result = []
    for c in chords:
        template = list(CHORD_TEMPLATES.get(c.quality, (0, 4, 7)))
        bass_oct = min(c.pitches) // 12

        r = rng.random()
        if r < 0.35 and len(template) <= 4:
            # Add an extension
            ext = rng.choice([14, 17, 21])  # 9, 11, 13
            template.append(ext)
        elif r < 0.6 and len(template) > 3:
            # Remove the highest extension
            template = template[:max(3, len(template) - 1)]
        # else: keep as-is

        new_pitches = voice_chord(c.root, tuple(template), bass_oct,
                                   style=style_profile.voicing_style,
                                   target_spread=int(style_profile.avg_spread),
                                   num_notes=max(3, len(c.pitches) + rng.randint(-1, 1)),
                                   rng=rng)

        vels = [max(20, min(120, int(style_profile.avg_velocity + rng.uniform(-10, 10))))
                for _ in new_pitches]

        result.append(ChordEvent(
            beat=c.beat, pitches=new_pitches, velocities=vels,
            duration=c.duration, root=c.root, quality=c.quality,
            extensions=list(c.extensions),
        ))
    return result


def condense_to_bars(chords, target_bars=4):
    """Condense a chord progression to fit within target_bars (in 4/4 time).

    Keeps the harmonic content but remaps timing so everything fits.
    Long MIDIs with 17 bars of chords become a tight 4-bar loop.
    """
    if not chords:
        return chords

    total_beats = target_bars * 4.0
    source_beats = max(c.beat + c.duration for c in chords)
    if source_beats <= 0:
        return chords

    # Strategy: keep all unique chord changes, space them evenly
    # Deduplicate consecutive identical chords (same root+quality)
    unique = [chords[0]]
    for c in chords[1:]:
        if c.root != unique[-1].root or c.quality != unique[-1].quality:
            unique.append(c)

    # Space chords evenly across target bars
    n = len(unique)
    beat_spacing = total_beats / n
    # Ensure minimum spacing of 1 beat for fast progressions
    beat_spacing = max(1.0, beat_spacing)

    result = []
    for i, c in enumerate(unique):
        new_beat = i * beat_spacing
        if new_beat >= total_beats:
            break
        # Duration fills until next chord or end
        next_beat = (i + 1) * beat_spacing if i + 1 < n else total_beats
        dur = min(next_beat - new_beat, total_beats - new_beat)

        result.append(ChordEvent(
            beat=new_beat, pitches=list(c.pitches), velocities=list(c.velocities),
            duration=dur, root=c.root, quality=c.quality,
            extensions=list(c.extensions),
        ))

    return result


def generate_b_section(chords, style_profile, rng):
    """Generate a contrasting B section by shifting to a related key area."""
    # Common B-section key moves: up a 4th, relative minor/major, up a step
    shifts = [5, 3, 9, 2, 7]  # P4, min3, maj6, maj2, P5
    shift = rng.choice(shifts)

    result = []
    for c in chords:
        new_root = (c.root + shift) % 12
        # Sometimes change quality too
        new_quality = c.quality
        if rng.random() < 0.3:
            if 'maj' in new_quality:
                new_quality = new_quality.replace('maj', 'min')
            elif 'min' in new_quality:
                new_quality = new_quality.replace('min', 'maj')

        template = CHORD_TEMPLATES.get(new_quality, CHORD_TEMPLATES.get(c.quality, (0, 4, 7)))
        bass_oct = min(c.pitches) // 12

        new_pitches = voice_chord(new_root, template, bass_oct,
                                   style=style_profile.voicing_style,
                                   target_spread=int(style_profile.avg_spread),
                                   num_notes=len(c.pitches), rng=rng)

        vels = [max(20, min(120, int(v + rng.uniform(-10, 10))))
                for v in c.velocities[:len(new_pitches)]]
        while len(vels) < len(new_pitches):
            vels.append(int(style_profile.avg_velocity))

        result.append(ChordEvent(
            beat=c.beat, pitches=new_pitches, velocities=vels,
            duration=c.duration, root=new_root, quality=new_quality,
            extensions=[],
        ))
    return result


# ---------------------------------------------------------------------------
# Pattern builder — chords to .song pattern data
# ---------------------------------------------------------------------------

def chords_to_pattern_lines(chords, steps_per_pattern=16, total_beats=None):
    """Convert ChordEvents into .song pattern melody lines.

    Quantizes chords to the step grid. Returns list of pattern strings,
    each a list of "m track=..." lines.
    """
    ticks_per_step = 0.25  # beats per 16th note step
    beats_per_pattern = steps_per_pattern * ticks_per_step

    if total_beats is None:
        total_beats = max((c.beat + c.duration for c in chords), default=beats_per_pattern)

    num_patterns = max(1, math.ceil(total_beats / beats_per_pattern))
    patterns = [[] for _ in range(num_patterns)]

    for c in chords:
        pat_idx = int(c.beat / beats_per_pattern)
        if pat_idx >= num_patterns:
            continue
        local_beat = c.beat - pat_idx * beats_per_pattern
        step = round(local_beat / ticks_per_step)
        if step >= steps_per_pattern:
            step = 0
            pat_idx += 1
            if pat_idx >= num_patterns:
                continue

        gate = max(1, round(c.duration / ticks_per_step))
        gate = min(gate, 64)

        # Write notes as melody track 2 (chord track) by default
        for i, (pitch, vel) in enumerate(zip(c.pitches, c.velocities)):
            nn = note_name(pitch)
            v = vel / 127.0
            line = f"m track=2 step={step} note={nn} vel={v:.3g} gate={gate}"
            patterns[pat_idx].append(line)

    return patterns


# ---------------------------------------------------------------------------
# .song file writer
# ---------------------------------------------------------------------------

def write_variation_song(filepath, bpm, style_profile, variation_sets,
                         variation_names, steps_per_pattern=16):
    """Write a .song file with the original + variations as patterns."""
    with open(filepath, 'w') as f:
        f.write("# PixelSynth DAW song — generated variations\n")
        f.write(f"# Source style: key={NOTE_NAMES[style_profile.key_root]} {style_profile.key_quality}\n")
        f.write(f"# Voicing: {style_profile.voicing_style}, rhythm: {style_profile.harmonic_rhythm}\n")
        f.write(f"# Avg chord size: {style_profile.avg_chord_size:.1f}, spread: {style_profile.avg_spread:.0f}st\n\n")

        # [song]
        f.write("[song]\n")
        f.write("format = 2\n")
        f.write(f'songName = "variations"\n')
        f.write(f"bpm = {bpm:g}\n")
        f.write(f"stepCount = {steps_per_pattern}\n")
        f.write("grooveSwing = 0\ngrooveJitter = 0\n")

        # [scale] — enable scale lock with detected key
        scale_type_map = {
            'major': 1, 'minor': 2, 'dorian': 5, 'mixolydian': 6,
            'lydian': 0, 'phrygian': 0, 'harmonic_minor': 7, 'melodic_minor': 0,
        }
        f.write(f"\n[scale]\n")
        f.write(f"enabled = false\n")
        f.write(f"root = {style_profile.key_root}\n")
        f.write(f"type = {scale_type_map.get(style_profile.key_quality, 0)}\n")

        # [groove]
        f.write(f"\n[groove]\n")
        for k in ['kickNudge', 'snareDelay', 'hatNudge', 'clapDelay', 'swing', 'jitter']:
            f.write(f"{k} = 0\n")
        for i in range(12):
            f.write(f"trackSwing{i} = 0\n")
            f.write(f"trackTranspose{i} = 0\n")
        f.write("melodyTimingJitter = 0\nmelodyVelocityJitter = 0\n")

        # [settings]
        f.write(f"\n[settings]\n")
        f.write(f"masterVol = 0.6\n")
        f.write(f"voiceRandomVowel = false\n")

        # [patch.*] — use Warm Pad (preset 131) for chord track, defaults for rest
        default_presets = [24, 25, 27, 26, 1, 1, 131, 1]
        for i in range(8):
            write_patch_section(f, f"patch.{i}", default_presets[i])

        # [bus.*]
        bus_names = ['drum0','drum1','drum2','drum3','bass','lead','chord','sampler']
        for bus in bus_names:
            f.write(f"\n[bus.{bus}]\n")
            f.write("volume = 0.8\npan = 0\nmute = false\nsolo = false\nreverbSend = 0.2\n")

        # [masterfx]
        f.write("\n[masterfx]\n")
        f.write("reverbEnabled = true\nreverbSize = 0.4\nreverbDamping = 0.5\n")
        f.write("reverbMix = 0.2\nreverbPreDelay = 0.03\n")
        f.write("compOn = true\ncompThreshold = -12\ncompRatio = 4\n")
        f.write("compAttack = 0.01\ncompRelease = 0.1\ncompMakeup = 0\n")

        # Build all pattern data
        all_patterns = []
        pattern_names = []

        for name, chord_set in zip(variation_names, variation_sets):
            # Find the total beat span
            if chord_set:
                total_beats = max(c.beat + c.duration for c in chord_set)
            else:
                total_beats = 16

            pat_lines = chords_to_pattern_lines(chord_set, steps_per_pattern, total_beats)
            for i, lines in enumerate(pat_lines):
                all_patterns.append(lines)
                suffix = f" pt{i+1}" if len(pat_lines) > 1 else ""
                pattern_names.append(f"{name}{suffix}")

        # Cap at 64 patterns
        all_patterns = all_patterns[:64]
        pattern_names = pattern_names[:64]

        # [arrangement]
        f.write(f"\n[arrangement]\n")
        f.write(f"length = {len(all_patterns)}\n")
        f.write(f"loopsPerPattern = 1\n")
        f.write(f"songMode = true\n")
        f.write(f"patterns = {' '.join(str(i) for i in range(len(all_patterns)))}\n")
        f.write(f"loops = {' '.join('1' for _ in all_patterns)}\n")

        # [pattern.N]
        for pi, (lines, pname) in enumerate(zip(all_patterns, pattern_names)):
            f.write(f"\n# --- {pname} ---\n")
            f.write(f"[pattern.{pi}]\n")
            f.write(f"drumTrackLength = 16 16 16 16\n")
            f.write(f"melodyTrackLength = 16 16 16\n")
            for line in lines:
                f.write(line + "\n")

    return len(all_patterns)


# ---------------------------------------------------------------------------
# Analysis output
# ---------------------------------------------------------------------------

def print_analysis(bpm, style_profile):
    """Print detailed chord analysis."""
    sp = style_profile
    print(f"\n{'='*60}")
    print(f"CHORD ANALYSIS")
    print(f"{'='*60}")
    print(f"Key:              {NOTE_NAMES[sp.key_root]} {sp.key_quality}")
    print(f"Scale:            {' '.join(NOTE_NAMES[(sp.key_root + iv) % 12] for iv in sp.scale)}")
    print(f"BPM:              {bpm:g}")
    print(f"Chords found:     {len(sp.chords)}")
    print(f"Avg chord size:   {sp.avg_chord_size:.1f} notes")
    print(f"Avg duration:     {sp.avg_duration:.1f} beats")
    print(f"Avg spread:       {sp.avg_spread:.0f} semitones")
    print(f"Harmonic rhythm:  {sp.harmonic_rhythm}")
    print(f"Voicing style:    {sp.voicing_style}")
    print(f"Velocity:         {sp.avg_velocity:.0f} (range {sp.vel_range[0]:.0f}-{sp.vel_range[1]:.0f})")

    print(f"\n{'---'*20}")
    print(f"{'Bar':>4} {'Beat':>5} {'Chord':<18} {'Notes':<35} {'Dur':>5} {'Spread':>6}")
    print(f"{'---'*20}")
    for c in sp.chords:
        names = ' '.join(note_name(p) for p in c.pitches)
        print(f"{c.bar:4d} {c.beat_in_bar:5.1f} {c.name:<18} {names:<35} {c.duration:5.1f} {c.spread:4d}st")

    # Progression summary
    print(f"\n--- Progression ---")
    bar_chords = defaultdict(list)
    for c in sp.chords:
        bar_chords[c.bar].append(c.name)
    for bar in sorted(bar_chords.keys()):
        print(f"  Bar {bar}: {' → '.join(bar_chords[bar])}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Analyze MIDI chords and generate variations for PixelSynth DAW')
    parser.add_argument('file', help='Input MIDI file (.mid)')
    parser.add_argument('-o', '--output', help='Output .song file')
    parser.add_argument('--analyze', action='store_true', help='Analysis only, no output')
    parser.add_argument('--variations', type=int, default=6,
                        help='Number of variation patterns to generate (default: 6)')
    parser.add_argument('--seed', type=int, default=None,
                        help='Random seed for reproducible output')
    parser.add_argument('--steps', type=int, default=16,
                        help='Steps per pattern (default: 16)')
    parser.add_argument('--bars', type=int, default=4,
                        help='Condense progression to N bars per variation (default: 4)')
    args = parser.parse_args()

    # Check for presets.txt
    presets_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'presets.txt')
    if not os.path.exists(presets_path):
        print("NOTE: presets.txt not found. Patches will use generic defaults.",
              file=sys.stderr)

    bpm, chords = parse_midi_chords(args.file)

    if not chords:
        print("No chords found in MIDI file.", file=sys.stderr)
        sys.exit(1)

    style = analyze_style(bpm, chords)
    print_analysis(bpm, style)

    if args.analyze:
        sys.exit(0)

    # Set up RNG
    rng = random.Random(args.seed)

    # Condense the original progression to fit target bars
    condensed = condense_to_bars(chords, target_bars=args.bars)
    print(f"\n--- Condensed to {args.bars} bars ({len(chords)} chords → {len(condensed)} unique changes) ---")
    for c in condensed:
        print(f"  beat {c.beat:5.1f}: {c.name}")

    # Generate variations from the condensed progression
    variation_sets = [condensed]  # pattern 0 = condensed original
    variation_names = ["original"]

    generators = [
        ("close voicing",    lambda: generate_revoicing(condensed, style, 'close', rng)),
        ("drop-2 voicing",   lambda: generate_revoicing(condensed, style, 'drop2', rng)),
        ("shell voicing",    lambda: generate_revoicing(condensed, style, 'shell', rng)),
        ("wide voicing",     lambda: generate_revoicing(condensed, style, 'wide', rng)),
        ("substitution",     lambda: generate_substitution(condensed, style, rng)),
        ("substitution 2",   lambda: generate_substitution(condensed, style, rng)),
        ("rhythm variation",  lambda: generate_rhythmic_variation(condensed, style, rng)),
        ("extension swap",   lambda: generate_extension_swap(condensed, style, rng)),
        ("B section (P4)",   lambda: generate_b_section(condensed, style, rng)),
        ("B section (rel)",  lambda: generate_b_section(condensed, style, rng)),
    ]

    count = min(args.variations, len(generators))
    for i in range(count):
        name, gen_fn = generators[i]
        var_chords = gen_fn()
        variation_sets.append(var_chords)
        variation_names.append(name)

    # Write output
    out_path = args.output
    if not out_path:
        base = os.path.splitext(args.file)[0]
        out_path = base + '-variations.song'

    num_patterns = write_variation_song(out_path, bpm, style, variation_sets,
                                         variation_names, args.steps)

    print(f"\n{'='*60}")
    print(f"Wrote {out_path}")
    print(f"  {len(variation_sets)} variations → {num_patterns} patterns")
    print(f"  Pattern 0: original")
    for i, name in enumerate(variation_names[1:], 1):
        print(f"  Pattern {i}: {name}")
    print(f"\nLoad in DAW: File → Load Song → select {os.path.basename(out_path)}")


if __name__ == '__main__':
    main()
