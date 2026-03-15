#!/usr/bin/env python3
"""audio_analyze.py — Extract musical features from an audio file.

Outputs tempo, key, chords (with timestamps), and a section summary.
Designed for AI agents to use when transcribing songs into the soundsystem.

Usage:
    python3 soundsystem/tools/audio_analyze.py <audio_file>
    python3 soundsystem/tools/audio_analyze.py <audio_file> --detailed
    python3 soundsystem/tools/audio_analyze.py <audio_file> --chords-only

Dependencies:
    pip3 install librosa numpy soundfile

Examples:
    python3 soundsystem/tools/audio_analyze.py song.mp3
    python3 soundsystem/tools/audio_analyze.py song.wav --detailed
"""

import sys
import argparse
import numpy as np

def load_audio(path, sr=22050):
    """Load audio file, return (samples, sample_rate)."""
    import librosa
    y, sr = librosa.load(path, sr=sr, mono=True)
    return y, sr

def detect_tempo(y, sr):
    """Estimate BPM."""
    import librosa
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    # librosa may return array
    if hasattr(tempo, '__len__'):
        tempo = float(tempo[0])
    return round(tempo, 1)

def detect_key(y, sr):
    """Estimate key using chroma features."""
    import librosa
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    chroma_avg = np.mean(chroma, axis=1)

    # Note names
    notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

    # Major and minor profiles (Krumhansl-Kessler)
    major_profile = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
    minor_profile = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])

    best_corr = -1
    best_key = 'C'
    best_mode = 'major'

    for shift in range(12):
        shifted = np.roll(chroma_avg, -shift)
        corr_maj = np.corrcoef(shifted, major_profile)[0, 1]
        corr_min = np.corrcoef(shifted, minor_profile)[0, 1]

        if corr_maj > best_corr:
            best_corr = corr_maj
            best_key = notes[shift]
            best_mode = 'major'
        if corr_min > best_corr:
            best_corr = corr_min
            best_key = notes[shift]
            best_mode = 'minor'

    return best_key, best_mode, best_corr

def detect_chords(y, sr, hop_length=4096):
    """Detect chords using chroma features and template matching.

    Returns list of (start_time, end_time, chord_name, confidence).
    """
    import librosa

    chroma = librosa.feature.chroma_cqt(y=y, sr=sr, hop_length=hop_length)
    times = librosa.frames_to_time(np.arange(chroma.shape[1]), sr=sr, hop_length=hop_length)

    notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

    # Chord templates: root position triads + 7ths
    templates = {}
    for i, note in enumerate(notes):
        # Major triad
        t = np.zeros(12)
        t[i] = 1.0; t[(i+4) % 12] = 0.8; t[(i+7) % 12] = 0.8
        templates[note] = t / np.linalg.norm(t)

        # Minor triad
        t = np.zeros(12)
        t[i] = 1.0; t[(i+3) % 12] = 0.8; t[(i+7) % 12] = 0.8
        templates[note + 'm'] = t / np.linalg.norm(t)

        # Dominant 7th
        t = np.zeros(12)
        t[i] = 1.0; t[(i+4) % 12] = 0.7; t[(i+7) % 12] = 0.7; t[(i+10) % 12] = 0.6
        templates[note + '7'] = t / np.linalg.norm(t)

        # Minor 7th
        t = np.zeros(12)
        t[i] = 1.0; t[(i+3) % 12] = 0.7; t[(i+7) % 12] = 0.7; t[(i+10) % 12] = 0.6
        templates[note + 'm7'] = t / np.linalg.norm(t)

        # Major 7th
        t = np.zeros(12)
        t[i] = 1.0; t[(i+4) % 12] = 0.7; t[(i+7) % 12] = 0.7; t[(i+11) % 12] = 0.6
        templates[note + 'maj7'] = t / np.linalg.norm(t)

        # Diminished
        t = np.zeros(12)
        t[i] = 1.0; t[(i+3) % 12] = 0.8; t[(i+6) % 12] = 0.8
        templates[note + 'dim'] = t / np.linalg.norm(t)

    # Match each frame to best chord
    raw_chords = []
    for frame_idx in range(chroma.shape[1]):
        frame = chroma[:, frame_idx]
        frame_norm = np.linalg.norm(frame)

        if frame_norm < 0.01:
            raw_chords.append(('N', 0.0))  # silence
            continue

        frame_normalized = frame / frame_norm
        best_chord = 'N'
        best_score = -1

        for name, template in templates.items():
            score = np.dot(frame_normalized, template)
            if score > best_score:
                best_score = score
                best_chord = name

        raw_chords.append((best_chord, best_score))

    # Consolidate: merge consecutive same-chord frames, require minimum duration
    min_duration = 0.3  # seconds
    chords = []
    if not raw_chords:
        return chords

    current_chord, current_conf = raw_chords[0]
    start_time = times[0] if len(times) > 0 else 0.0

    for idx in range(1, len(raw_chords)):
        chord, conf = raw_chords[idx]
        t = times[idx] if idx < len(times) else times[-1]

        if chord != current_chord:
            duration = t - start_time
            if duration >= min_duration and current_chord != 'N':
                chords.append((start_time, t, current_chord, current_conf))
            current_chord = chord
            current_conf = conf
            start_time = t
        else:
            current_conf = max(current_conf, conf)

    # Final chord
    end_time = len(y) / sr
    duration = end_time - start_time
    if duration >= min_duration and current_chord != 'N':
        chords.append((start_time, end_time, current_chord, current_conf))

    return chords

def simplify_chords(chords, min_duration=1.0):
    """Merge short chord segments and filter by minimum duration."""
    if not chords:
        return chords

    simplified = []
    current = list(chords[0])

    for i in range(1, len(chords)):
        start, end, name, conf = chords[i]
        # Merge if same chord and gap is tiny
        if name == current[2] and (start - current[1]) < 0.3:
            current[1] = end
            current[3] = max(current[3], conf)
        else:
            if (current[1] - current[0]) >= min_duration:
                simplified.append(tuple(current))
            current = [start, end, name, conf]

    if (current[1] - current[0]) >= min_duration:
        simplified.append(tuple(current))

    return simplified

def detect_bass_notes(y, sr, hop_length=4096):
    """Detect prominent bass notes using low-frequency chroma."""
    import librosa
    # Isolate low frequencies (below ~300 Hz)
    y_bass = librosa.effects.harmonic(y)
    chroma_bass = librosa.feature.chroma_cqt(y=y_bass, sr=sr, hop_length=hop_length,
                                               fmin=librosa.note_to_hz('C1'),
                                               n_octaves=3)
    times = librosa.frames_to_time(np.arange(chroma_bass.shape[1]), sr=sr, hop_length=hop_length)

    notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    bass_notes = []

    for idx in range(chroma_bass.shape[1]):
        frame = chroma_bass[:, idx]
        if np.max(frame) > 0.3:
            note_idx = np.argmax(frame)
            bass_notes.append((times[idx], notes[note_idx], float(frame[note_idx])))

    return bass_notes

def format_time(seconds):
    """Format seconds as M:SS.s"""
    m = int(seconds // 60)
    s = seconds % 60
    return f"{m}:{s:04.1f}"

def analyze(path, detailed=False, chords_only=False):
    """Main analysis pipeline."""
    print(f"Loading: {path}")
    y, sr = load_audio(path)
    duration = len(y) / sr
    print(f"Duration: {format_time(duration)} ({duration:.1f}s)")
    print()

    # Tempo
    tempo = detect_tempo(y, sr)
    print(f"Tempo: ~{tempo} BPM")

    # Key
    key, mode, confidence = detect_key(y, sr)
    print(f"Key: {key} {mode} (confidence: {confidence:.2f})")
    print()

    # Chords
    print("=== Chord Progression ===")
    raw_chords = detect_chords(y, sr)
    chords = simplify_chords(raw_chords, min_duration=0.5)

    if chords_only:
        for start, end, name, conf in chords:
            print(f"  {format_time(start)} - {format_time(end)}  {name:8s} ({conf:.2f})")
        return

    for start, end, name, conf in chords:
        print(f"  {format_time(start)} - {format_time(end)}  {name:8s} ({conf:.2f})")

    # Chord summary (unique progression)
    print()
    print("=== Chord Summary (unique sequence) ===")
    seen = []
    for _, _, name, _ in chords:
        if not seen or seen[-1] != name:
            seen.append(name)
    # Print in groups of 4
    for i in range(0, len(seen), 4):
        chunk = seen[i:i+4]
        print(f"  | {'  '.join(f'{c:8s}' for c in chunk)} |")

    if detailed:
        print()
        print("=== Bass Notes (prominent) ===")
        bass = detect_bass_notes(y, sr)
        # Summarize: most common bass notes per ~2s window
        window = 2.0
        t = 0.0
        while t < duration:
            window_notes = [n for time, n, _ in bass if t <= time < t + window]
            if window_notes:
                from collections import Counter
                most_common = Counter(window_notes).most_common(1)[0][0]
                print(f"  {format_time(t)}: {most_common}")
            t += window

    # Section detection via RMS energy
    print()
    print("=== Energy Sections ===")
    import librosa
    rms = librosa.feature.rms(y=y, frame_length=2048, hop_length=512)[0]
    rms_times = librosa.frames_to_time(np.arange(len(rms)), sr=sr, hop_length=512)
    # Smooth RMS over ~2s windows
    smooth_window = int(2.0 * sr / 512)
    if smooth_window > 1:
        rms_smooth = np.convolve(rms, np.ones(smooth_window)/smooth_window, mode='same')
    else:
        rms_smooth = rms
    avg_rms = np.mean(rms_smooth)
    for t in np.arange(0, duration, 4.0):
        idx_start = np.searchsorted(rms_times, t)
        idx_end = np.searchsorted(rms_times, t + 4.0)
        if idx_start < len(rms_smooth):
            seg_rms = np.mean(rms_smooth[idx_start:idx_end]) if idx_end > idx_start else 0
            bar = '#' * int(seg_rms / avg_rms * 20)
            print(f"  {format_time(t)}: {bar}")

def main():
    parser = argparse.ArgumentParser(description='Analyze audio for musical features')
    parser.add_argument('audio_file', help='Path to audio file (MP3, WAV, etc.)')
    parser.add_argument('--detailed', action='store_true', help='Include bass note tracking')
    parser.add_argument('--chords-only', action='store_true', help='Output only chord timeline')
    args = parser.parse_args()

    analyze(args.audio_file, detailed=args.detailed, chords_only=args.chords_only)

if __name__ == '__main__':
    main()
