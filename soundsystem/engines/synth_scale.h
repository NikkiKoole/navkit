// PixelSynth - Scale Lock System
// Scale types, constraint functions, MIDI-to-frequency with scale quantization
// Extracted from synth.h — include after synth globals are defined

#ifndef PIXELSYNTH_SYNTH_SCALE_H
#define PIXELSYNTH_SYNTH_SCALE_H

// ============================================================================
// SCALE LOCK SYSTEM
// ============================================================================

typedef enum {
    SCALE_CHROMATIC,    // All 12 notes (no constraint)
    SCALE_MAJOR,        // Major scale (Ionian)
    SCALE_MINOR,        // Natural minor (Aeolian)
    SCALE_PENTATONIC,   // Major pentatonic
    SCALE_MINOR_PENTA,  // Minor pentatonic
    SCALE_BLUES,        // Blues scale
    SCALE_DORIAN,       // Dorian mode
    SCALE_MIXOLYDIAN,   // Mixolydian mode
    SCALE_HARMONIC_MIN, // Harmonic minor
    SCALE_COUNT
} ScaleType;

static const char* scaleNames[] = {
    "Chromatic", "Major", "Minor", "Penta", "MinPenta", 
    "Blues", "Dorian", "Mixolyd", "HarmMin"
};

// Scale intervals (1 = note in scale, 0 = not in scale)
// Index 0 = root, 1 = minor 2nd, 2 = major 2nd, etc.
static const int scaleIntervals[SCALE_COUNT][12] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},  // Chromatic
    {1,0,1,0,1,1,0,1,0,1,0,1},  // Major: C D E F G A B
    {1,0,1,1,0,1,0,1,1,0,1,0},  // Minor: C D Eb F G Ab Bb
    {1,0,1,0,1,0,0,1,0,1,0,0},  // Pentatonic: C D E G A
    {1,0,0,1,0,1,0,1,0,0,1,0},  // Minor Pentatonic: C Eb F G Bb
    {1,0,0,1,0,1,1,1,0,0,1,0},  // Blues: C Eb F F# G Bb
    {1,0,1,1,0,1,0,1,0,1,1,0},  // Dorian: C D Eb F G A Bb
    {1,0,1,0,1,1,0,1,0,1,1,0},  // Mixolydian: C D E F G A Bb
    {1,0,1,1,0,1,0,1,1,0,0,1},  // Harmonic Minor: C D Eb F G Ab B
};

// Root note names
static const char* rootNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Constrain a MIDI note to the current scale
// Returns the nearest note in the scale (checks below first as tiebreaker)
static int constrainToScale(int midiNote) {
    if (!scaleLockEnabled || scaleType == SCALE_CHROMATIC) return midiNote;
    
    // Get the note's position relative to root (0-11)
    int noteInOctave = ((midiNote % 12) - scaleRoot + 12) % 12;
    int octave = midiNote / 12;
    
    // If already in scale, return as-is
    if (scaleIntervals[scaleType][noteInOctave]) return midiNote;
    
    // Find nearest note in scale (prefer going down)
    for (int offset = 1; offset < 12; offset++) {
        // Check below first
        int below = (noteInOctave - offset + 12) % 12;
        if (scaleIntervals[scaleType][below]) {
            int result = octave * 12 + ((below + scaleRoot) % 12);
            // Handle octave boundary
            if (below > noteInOctave) result -= 12;
            return result;
        }
        // Then check above
        int above = (noteInOctave + offset) % 12;
        if (scaleIntervals[scaleType][above]) {
            int result = octave * 12 + ((above + scaleRoot) % 12);
            // Handle octave boundary
            if (above < noteInOctave) result += 12;
            return result;
        }
    }
    
    return midiNote;  // Fallback (shouldn't happen)
}

// Convert MIDI note to frequency with optional scale lock
__attribute__((unused))
static float midiToFreqScaled(int midiNote) {
    int constrained = constrainToScale(midiNote);
    return 440.0f * powf(2.0f, (constrained - 69) / 12.0f);
}

// Get scale degree (1-7 for diatonic scales, 0 if not in scale)
__attribute__((unused))
static int getScaleDegree(int midiNote) {
    if (scaleType == SCALE_CHROMATIC) return (midiNote % 12) + 1;
    
    int noteInOctave = ((midiNote % 12) - scaleRoot + 12) % 12;
    if (!scaleIntervals[scaleType][noteInOctave]) return 0;
    
    // Count how many scale notes are below this one
    int degree = 1;
    for (int i = 0; i < noteInOctave; i++) {
        if (scaleIntervals[scaleType][i]) degree++;
    }
    return degree;
}

// Check if a note is in the current scale
__attribute__((unused))
static bool isInScale(int midiNote) {
    if (!scaleLockEnabled || scaleType == SCALE_CHROMATIC) return true;
    int noteInOctave = ((midiNote % 12) - scaleRoot + 12) % 12;
    return scaleIntervals[scaleType][noteInOctave] != 0;
}


#endif // PIXELSYNTH_SYNTH_SCALE_H
