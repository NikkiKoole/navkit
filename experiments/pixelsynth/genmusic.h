// Generative Music System for PixelSynth
// Include this after defining Voice, WaveType, VowelType, and drum functions

#ifndef GENMUSIC_H
#define GENMUSIC_H

// ============================================================================
// GENERATIVE MUSIC SYSTEM
// ============================================================================

// Musical scales (semitone offsets from root)
static const int SCALE_MINOR[] = {0, 2, 3, 5, 7, 8, 10};      // Natural minor
static const int SCALE_PENTA[] = {0, 3, 5, 7, 10};            // Minor pentatonic
static const int SCALE_BLUES[] = {0, 3, 5, 6, 7, 10};         // Blues
static const int SCALE_MAJOR[] = {0, 2, 4, 5, 7, 9, 11};      // Major
static const int SCALE_DORIAN[] = {0, 2, 3, 5, 7, 9, 10};     // Dorian (jazzy minor)
static const int SCALE_MIXO[] = {0, 2, 4, 5, 7, 9, 10};       // Mixolydian (dominant)
static const int SCALE_PHRYGIAN[] = {0, 1, 3, 5, 7, 8, 10};   // Phrygian (spanish)
static const int SCALE_HARMMIN[] = {0, 2, 3, 5, 7, 8, 11};    // Harmonic minor

// Scale info for UI
typedef struct {
    const int *notes;
    int length;
    const char *name;
} ScaleInfo;

#define NUM_SCALES 8
static const ScaleInfo SCALES[NUM_SCALES] = {
    { SCALE_MINOR,    7, "Minor" },
    { SCALE_MAJOR,    7, "Major" },
    { SCALE_PENTA,    5, "Pentatonic" },
    { SCALE_BLUES,    6, "Blues" },
    { SCALE_DORIAN,   7, "Dorian" },
    { SCALE_MIXO,     7, "Mixolydian" },
    { SCALE_PHRYGIAN, 7, "Phrygian" },
    { SCALE_HARMMIN,  7, "Harm Minor" },
};

// Root note names
static const char *ROOT_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Music styles
typedef enum {
    STYLE_CLAUDISH,    // Original eclectic style
    STYLE_HOUSE,       // 4-on-floor, 303 bass, minimal melody
    STYLE_HIPHOP,      // Dilla-style with micro-timing
    STYLE_COUNT
} MusicStyle;

static const char *STYLE_NAMES[] = {"Claudish", "House", "Hip-Hop"};

// Dilla-style micro-timing offsets (in fractions of a 16th note)
// Some drums early, some late - creates that drunk/loose feel
static const float DILLA_TIMING[16] = {
    0.0f,    // 1 - kick on time
    0.08f,   // e - slightly late
    -0.05f,  // + - slightly early  
    0.12f,   // a - late (lazy)
    0.0f,    // 2 - snare on time
    0.1f,    // e - late
    -0.03f,  // + - early
    0.15f,   // a - very late (signature Dilla)
    0.0f,    // 3 - on time
    0.06f,   // e - late
    -0.04f,  // + - early
    0.1f,    // a - late
    0.0f,    // 4 - snare on time
    0.08f,   // e - late
    -0.02f,  // + - slightly early
    0.18f,   // a - very late
};

// Generative music state
// Instrument parameters (tweakable via UI)
typedef struct {
    int wave;           // WaveType
    float volume;
    float attack;
    float decay;
    float sustain;
    float release;
    float filterCutoff;
    float pulseWidth;   // For square wave
    float pitchOctave;  // Octave offset (-2 to +2)
} GenInstParams;

typedef struct {
    bool active;
    int style;                // MusicStyle
    float bpm;
    float swing;              // 0 = straight, 0.3 = triplet feel
    float timer;
    int tick;                 // 16th note counter (0-15 per bar)
    int bar;                  // Bar counter
    float microTiming[16];    // Per-tick timing offsets
    
    // Musical key
    int rootNote;             // MIDI note number (e.g., 36 = C2)
    int scaleIndex;           // Index into SCALES array
    const int *scale;
    int scaleLen;
    
    // Pattern probabilities
    float kickProb;
    float snareProb;
    float hhProb;
    float clapProb;
    float bassProb;
    float melodyProb;
    float voiceProb;
    
    // Current chord (for harmony)
    int chordRoot;            // Offset from key root
    int chordType;            // 0=minor, 1=major, 2=dim, etc.
    
    // Instrument parameters (tweakable)
    GenInstParams bass;
    GenInstParams melody;
    GenInstParams vox;
    
    // Voice indices for active sounds
    int melodyVoice;
    int bassVoice;
    int voxVoice;
    
    // State
    int lastMelodyNote;
    int lastBassNote;
    float bassSlide;          // For 303-style slides
    
    // Pattern memory (for repetition/variation)
    unsigned char drumPattern[16];
    unsigned char bassPattern[16];
    int patternVariation;
} GenMusic;

static GenMusic genMusic = {0};

// Convert scale degree to frequency
static float scaleToFreq(int root, const int *scale, int scaleLen, int degree) {
    int octave = degree / scaleLen;
    int note = degree % scaleLen;
    if (note < 0) { note += scaleLen; octave--; }
    int midi = root + scale[note] + octave * 12;
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

// Random int in range using the existing noise generator
static int rndInt(int min, int max) {
    noiseState = noiseState * 1103515245 + 12345;
    return min + ((noiseState >> 16) % (max - min + 1));
}

// Random float 0-1
static float rndFloat(void) {
    noiseState = noiseState * 1103515245 + 12345;
    return (float)(noiseState >> 16) / 65535.0f;
}

// Set scale by index
static void setGenMusicScale(int index) {
    if (index < 0) index = NUM_SCALES - 1;
    if (index >= NUM_SCALES) index = 0;
    genMusic.scaleIndex = index;
    genMusic.scale = SCALES[index].notes;
    genMusic.scaleLen = SCALES[index].length;
}

// Set root note (0-11 for C through B, will be set to octave 2)
static void setGenMusicRoot(int root) {
    if (root < 0) root = 11;
    if (root > 11) root = 0;
    genMusic.rootNote = 36 + root;  // C2 + semitones
}

// Initialize instrument params to defaults
static void initBassParams(GenInstParams *p, int style) {
    switch (style) {
        case STYLE_HOUSE:
            p->wave = WAVE_SQUARE;
            p->volume = 0.5f;
            p->attack = 0.005f;
            p->decay = 0.2f;
            p->sustain = 0.0f;
            p->release = 0.1f;
            p->filterCutoff = 0.35f;
            p->pulseWidth = 0.5f;
            p->pitchOctave = 0.0f;
            break;
        case STYLE_HIPHOP:
            p->wave = WAVE_TRIANGLE;
            p->volume = 0.5f;
            p->attack = 0.02f;
            p->decay = 0.2f;
            p->sustain = 0.5f;
            p->release = 0.2f;
            p->filterCutoff = 0.25f;
            p->pulseWidth = 0.5f;
            p->pitchOctave = 0.0f;
            break;
        default: // CLAUDISH
            p->wave = WAVE_SAW;
            p->volume = 0.5f;
            p->attack = 0.01f;
            p->decay = 0.1f;
            p->sustain = 0.6f;
            p->release = 0.15f;
            p->filterCutoff = 0.3f;
            p->pulseWidth = 0.5f;
            p->pitchOctave = 0.0f;
            break;
    }
}

static void initMelodyParams(GenInstParams *p, int style) {
    (void)style;  // Currently same for all styles
    p->wave = WAVE_SQUARE;
    p->volume = 0.35f;
    p->attack = 0.02f;
    p->decay = 0.15f;
    p->sustain = 0.4f;
    p->release = 0.3f;
    p->filterCutoff = 0.6f;
    p->pulseWidth = 0.3f;
    p->pitchOctave = 2.0f;  // 2 octaves up from bass
}

static void initVoxParams(GenInstParams *p, int style) {
    (void)style;
    p->wave = WAVE_VOICE;
    p->volume = 0.4f;
    p->attack = 0.02f;
    p->decay = 0.05f;
    p->sustain = 0.7f;
    p->release = 0.25f;
    p->filterCutoff = 0.7f;
    p->pulseWidth = 0.5f;
    p->pitchOctave = 1.0f;  // 1 octave up from bass
}

// Set style and configure defaults for it
static void setGenMusicStyle(int style) {
    if (style < 0) style = STYLE_COUNT - 1;
    if (style >= STYLE_COUNT) style = 0;
    genMusic.style = style;
    
    // Clear micro-timing
    for (int i = 0; i < 16; i++) genMusic.microTiming[i] = 0.0f;
    
    // Set style-specific instrument sounds
    initBassParams(&genMusic.bass, style);
    initMelodyParams(&genMusic.melody, style);
    initVoxParams(&genMusic.vox, style);
    
    switch (style) {
        case STYLE_CLAUDISH:
            genMusic.bpm = 110.0f;
            genMusic.swing = 0.1f;
            genMusic.kickProb = 0.9f;
            genMusic.snareProb = 0.8f;
            genMusic.hhProb = 0.7f;
            genMusic.clapProb = 0.3f;
            genMusic.bassProb = 0.6f;
            genMusic.melodyProb = 0.4f;
            genMusic.voiceProb = 0.15f;
            break;
            
        case STYLE_HOUSE:
            genMusic.bpm = 124.0f;
            genMusic.swing = 0.0f;
            genMusic.kickProb = 1.0f;
            genMusic.snareProb = 0.0f;
            genMusic.hhProb = 0.9f;
            genMusic.clapProb = 0.95f;
            genMusic.bassProb = 0.8f;
            genMusic.melodyProb = 0.25f;
            genMusic.voiceProb = 0.05f;
            break;
            
        case STYLE_HIPHOP:
            genMusic.bpm = 88.0f;
            genMusic.swing = 0.0f;
            genMusic.kickProb = 0.95f;
            genMusic.snareProb = 0.9f;
            genMusic.hhProb = 0.6f;
            genMusic.clapProb = 0.1f;
            genMusic.bassProb = 0.5f;
            genMusic.melodyProb = 0.3f;
            genMusic.voiceProb = 0.1f;
            for (int i = 0; i < 16; i++) {
                genMusic.microTiming[i] = DILLA_TIMING[i];
            }
            break;
    }
}

// Initialize generative music
static void initGenMusic(void) {
    genMusic.style = STYLE_CLAUDISH;
    genMusic.bpm = 110.0f;
    genMusic.swing = 0.1f;
    genMusic.rootNote = 36;   // C2
    genMusic.scaleIndex = 0;
    genMusic.scale = SCALES[0].notes;
    genMusic.scaleLen = SCALES[0].length;
    
    genMusic.kickProb = 0.9f;
    genMusic.snareProb = 0.8f;
    genMusic.hhProb = 0.7f;
    genMusic.clapProb = 0.3f;
    genMusic.bassProb = 0.6f;
    genMusic.melodyProb = 0.4f;
    genMusic.voiceProb = 0.15f;
    
    // Initialize instrument parameters
    initBassParams(&genMusic.bass, STYLE_CLAUDISH);
    initMelodyParams(&genMusic.melody, STYLE_CLAUDISH);
    initVoxParams(&genMusic.vox, STYLE_CLAUDISH);
    
    genMusic.lastMelodyNote = 7;
    genMusic.melodyVoice = -1;
    genMusic.bassVoice = -1;
    genMusic.voxVoice = -1;
    genMusic.lastBassNote = 0;
    genMusic.bassSlide = 0.0f;
    
    for (int i = 0; i < 16; i++) genMusic.microTiming[i] = 0.0f;
    
    genMusic.patternVariation = 0;
}

// Generate drum pattern for a bar (style-dependent)
static void generateDrumPattern(void) {
    for (int i = 0; i < 16; i++) {
        genMusic.drumPattern[i] = 0;
    }
    
    switch (genMusic.style) {
        case STYLE_CLAUDISH: {
            // Original eclectic style - variable kick patterns
            int kickStyle = rndInt(0, 3);
            switch (kickStyle) {
                case 0: // Four on floor
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[4] |= 1;
                    genMusic.drumPattern[8] |= 1;
                    genMusic.drumPattern[12] |= 1;
                    break;
                case 1: // Boom-bap
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[10] |= 1;
                    break;
                case 2: // Syncopated
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[6] |= 1;
                    genMusic.drumPattern[10] |= 1;
                    break;
                case 3: // Minimal
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[8] |= 1;
                    break;
            }
            // Snare backbeat
            genMusic.drumPattern[4] |= 2;
            genMusic.drumPattern[12] |= 2;
            if (rndFloat() < 0.3f) genMusic.drumPattern[14] |= 2;
            // Variable hihats
            int hhStyle = rndInt(0, 2);
            for (int i = 0; i < 16; i++) {
                if (hhStyle == 0 && (i % 2 == 0)) genMusic.drumPattern[i] |= 4;
                if (hhStyle == 1) genMusic.drumPattern[i] |= 4;
                if (hhStyle == 2 && (i % 4 == 0 || i % 4 == 3)) genMusic.drumPattern[i] |= 4;
            }
            if (rndFloat() < 0.5f) {
                int openPos = rndInt(0, 3) * 4 + 2;
                genMusic.drumPattern[openPos] |= 8;
                genMusic.drumPattern[openPos] &= ~4;
            }
            break;
        }
        
        case STYLE_HOUSE: {
            // Four-on-floor kick - always!
            genMusic.drumPattern[0] |= 1;
            genMusic.drumPattern[4] |= 1;
            genMusic.drumPattern[8] |= 1;
            genMusic.drumPattern[12] |= 1;
            
            // Clap on 2 and 4 (use snare bit but we'll trigger clap in update)
            genMusic.drumPattern[4] |= 2;
            genMusic.drumPattern[12] |= 2;
            
            // Offbeat hihats (classic house)
            for (int i = 0; i < 16; i++) {
                if (i % 2 == 1) genMusic.drumPattern[i] |= 4;  // Offbeat 8ths
            }
            // Open hihat on the "and" of 2 and 4
            genMusic.drumPattern[6] |= 8;
            genMusic.drumPattern[6] &= ~4;
            genMusic.drumPattern[14] |= 8;
            genMusic.drumPattern[14] &= ~4;
            break;
        }
        
        case STYLE_HIPHOP: {
            // Boom-bap style kick patterns
            int kickStyle = rndInt(0, 2);
            switch (kickStyle) {
                case 0: // Classic boom-bap
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[10] |= 1;
                    break;
                case 1: // With extra kick
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[6] |= 1;
                    genMusic.drumPattern[10] |= 1;
                    break;
                case 2: // Sparse
                    genMusic.drumPattern[0] |= 1;
                    genMusic.drumPattern[14] |= 1;
                    break;
            }
            
            // Snare on 2 and 4, sometimes ghost notes
            genMusic.drumPattern[4] |= 2;
            genMusic.drumPattern[12] |= 2;
            if (rndFloat() < 0.4f) genMusic.drumPattern[7] |= 2;   // Ghost
            if (rndFloat() < 0.3f) genMusic.drumPattern[15] |= 2;  // Ghost
            
            // Sparse hihats with variation
            int hhPattern = rndInt(0, 2);
            if (hhPattern == 0) {
                // 8ths
                for (int i = 0; i < 16; i += 2) genMusic.drumPattern[i] |= 4;
            } else if (hhPattern == 1) {
                // Syncopated
                genMusic.drumPattern[0] |= 4;
                genMusic.drumPattern[3] |= 4;
                genMusic.drumPattern[6] |= 4;
                genMusic.drumPattern[8] |= 4;
                genMusic.drumPattern[11] |= 4;
                genMusic.drumPattern[14] |= 4;
            } else {
                // Sparse
                genMusic.drumPattern[0] |= 4;
                genMusic.drumPattern[4] |= 4;
                genMusic.drumPattern[8] |= 4;
                genMusic.drumPattern[12] |= 4;
            }
            break;
        }
    }
}

// Generate bass pattern (style-dependent)
static void generateBassPattern(void) {
    for (int i = 0; i < 16; i++) {
        genMusic.bassPattern[i] = 0;
    }
    
    switch (genMusic.style) {
        case STYLE_CLAUDISH: {
            int pattern = rndInt(0, 3);
            switch (pattern) {
                case 0: // Root on downbeats
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[8] = 1;
                    break;
                case 1: // Octave bounce
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[4] = 2;
                    genMusic.bassPattern[8] = 1;
                    genMusic.bassPattern[12] = 2;
                    break;
                case 2: // Busy
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[3] = 1;
                    genMusic.bassPattern[6] = 2;
                    genMusic.bassPattern[8] = 1;
                    genMusic.bassPattern[11] = 1;
                    genMusic.bassPattern[14] = 2;
                    break;
                case 3: // Syncopated
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[3] = 1;
                    genMusic.bassPattern[10] = 1;
                    break;
            }
            break;
        }
        
        case STYLE_HOUSE: {
            // 303-style patterns - busy 16th note lines
            int pattern = rndInt(0, 2);
            switch (pattern) {
                case 0: // Classic 303 - running 16ths with accents
                    for (int i = 0; i < 16; i++) {
                        genMusic.bassPattern[i] = 1;
                    }
                    // Mark accents (higher values = accent)
                    genMusic.bassPattern[0] = 3;   // Strong accent
                    genMusic.bassPattern[4] = 2;   // Medium
                    genMusic.bassPattern[8] = 3;   // Strong
                    genMusic.bassPattern[12] = 2;  // Medium
                    break;
                case 1: // Syncopated 303
                    genMusic.bassPattern[0] = 3;
                    genMusic.bassPattern[2] = 1;
                    genMusic.bassPattern[3] = 2;
                    genMusic.bassPattern[6] = 1;
                    genMusic.bassPattern[7] = 2;
                    genMusic.bassPattern[8] = 3;
                    genMusic.bassPattern[10] = 1;
                    genMusic.bassPattern[11] = 2;
                    genMusic.bassPattern[14] = 1;
                    genMusic.bassPattern[15] = 2;
                    break;
                case 2: // Pumping bass
                    genMusic.bassPattern[0] = 3;
                    genMusic.bassPattern[2] = 1;
                    genMusic.bassPattern[4] = 2;
                    genMusic.bassPattern[6] = 1;
                    genMusic.bassPattern[8] = 3;
                    genMusic.bassPattern[10] = 1;
                    genMusic.bassPattern[12] = 2;
                    genMusic.bassPattern[14] = 1;
                    break;
            }
            break;
        }
        
        case STYLE_HIPHOP: {
            // Sparse, melodic bass with space
            int pattern = rndInt(0, 2);
            switch (pattern) {
                case 0: // Minimal
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[10] = 1;
                    break;
                case 1: // With fill
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[6] = 1;
                    genMusic.bassPattern[10] = 1;
                    genMusic.bassPattern[14] = 2;
                    break;
                case 2: // Walking
                    genMusic.bassPattern[0] = 1;
                    genMusic.bassPattern[4] = 1;
                    genMusic.bassPattern[8] = 1;
                    genMusic.bassPattern[12] = 2;
                    break;
            }
            break;
        }
    }
}

// Play a bass note using genMusic.bass params
// accent: 0=normal, 1=soft, 2=medium, 3=hard (affects volume/filter for 303-style)
static void playBassNote(int degree, int accent) {
    if (genMusic.bassVoice >= 0) {
        releaseNote(genMusic.bassVoice);
    }
    
    GenInstParams *p = &genMusic.bass;
    
    // Apply pitch octave offset
    int octaveOffset = (int)(p->pitchOctave * 12.0f);
    float freq = scaleToFreq(genMusic.rootNote + octaveOffset, genMusic.scale, genMusic.scaleLen, degree);
    
    int v = findVoice();
    Voice *voice = &voices[v];
    
    float oldFilterLp = voice->filterLp;
    voice->frequency = freq;
    voice->baseFrequency = freq;
    voice->phase = 0.0f;
    
    // Use tweakable params
    voice->wave = (WaveType)p->wave;
    voice->pulseWidth = p->pulseWidth;
    voice->attack = p->attack;
    voice->decay = p->decay;
    voice->sustain = p->sustain;
    voice->release = p->release;
    
    // Accent modifies volume and filter (303-style)
    float accentMod = accent * 0.1f;
    voice->volume = p->volume + accentMod;
    voice->filterCutoff = p->filterCutoff + accentMod * 0.5f;
    if (voice->filterCutoff > 1.0f) voice->filterCutoff = 1.0f;
    
    voice->pwmRate = 0.0f;
    voice->pwmDepth = 0.0f;
    voice->vibratoRate = 0.0f;
    voice->vibratoDepth = 0.0f;
    voice->envPhase = 0.0f;
    voice->envLevel = 0.0f;
    voice->envStage = 1;
    voice->filterLp = oldFilterLp * 0.3f;
    voice->pitchSlide = 0.0f;
    voice->arpEnabled = false;
    
    genMusic.bassVoice = v;
    genMusic.lastBassNote = degree;
}

// Play a melody note using genMusic.melody params
static void playMelodyNote(int degree) {
    if (genMusic.melodyVoice >= 0) {
        releaseNote(genMusic.melodyVoice);
    }
    
    GenInstParams *p = &genMusic.melody;
    
    // Apply pitch octave offset
    int octaveOffset = (int)(p->pitchOctave * 12.0f);
    float freq = scaleToFreq(genMusic.rootNote + octaveOffset, genMusic.scale, genMusic.scaleLen, degree);
    
    int v = findVoice();
    Voice *voice = &voices[v];
    
    float oldFilterLp = voice->filterLp;
    voice->frequency = freq;
    voice->baseFrequency = freq;
    voice->phase = 0.0f;
    
    // Use tweakable params
    voice->wave = (WaveType)p->wave;
    voice->volume = p->volume;
    voice->pulseWidth = p->pulseWidth;
    voice->attack = p->attack;
    voice->decay = p->decay;
    voice->sustain = p->sustain;
    voice->release = p->release;
    voice->filterCutoff = p->filterCutoff;
    
    // Some nice defaults for melody
    voice->pwmRate = 4.0f;
    voice->pwmDepth = 0.15f;
    voice->vibratoRate = 5.0f;
    voice->vibratoDepth = 0.2f;
    voice->envPhase = 0.0f;
    voice->envLevel = 0.0f;
    voice->envStage = 1;
    voice->filterLp = oldFilterLp * 0.3f;
    voice->pitchSlide = 0.0f;
    voice->arpEnabled = false;
    
    genMusic.melodyVoice = v;
    genMusic.lastMelodyNote = degree;
}

// Play a vocal note using genMusic.vox params
static void playVoxNote(int degree) {
    if (genMusic.voxVoice >= 0) {
        releaseNote(genMusic.voxVoice);
    }
    
    GenInstParams *p = &genMusic.vox;
    
    // Apply pitch octave offset
    int octaveOffset = (int)(p->pitchOctave * 12.0f);
    float freq = scaleToFreq(genMusic.rootNote + octaveOffset, genMusic.scale, genMusic.scaleLen, degree);
    
    VowelType vowel = (VowelType)rndInt(0, VOWEL_COUNT - 1);
    genMusic.voxVoice = playVowel(freq, vowel);
}

// Update generative music (call every frame)
// Requires: drumKick, drumSnare, drumClap, drumClosedHH, drumOpenHH, drumRimshot, drumCowbell
static void updateGenMusic(float dt) {
    if (!genMusic.active) return;
    
    // Calculate tick timing
    float tickDuration = 60.0f / genMusic.bpm / 4.0f;  // 16th note
    
    genMusic.timer += dt;
    
    while (genMusic.timer >= tickDuration) {
        genMusic.timer -= tickDuration;
        
        int tick = genMusic.tick;
        
        // Apply timing modifications based on style
        float timingOffset = 0.0f;
        
        if (genMusic.style == STYLE_HIPHOP) {
            // Dilla-style micro-timing - apply offset for this tick
            timingOffset = genMusic.microTiming[tick] * tickDuration;
            genMusic.timer -= timingOffset;
        } else if (genMusic.swing > 0.0f && (tick % 2 == 1)) {
            // Traditional swing for other styles
            genMusic.timer -= tickDuration * genMusic.swing;
        }
        
        // === DRUMS ===
        unsigned char dp = genMusic.drumPattern[tick];
        
        if ((dp & 1) && rndFloat() < genMusic.kickProb) {
            drumKick();
        }
        if ((dp & 2) && rndFloat() < genMusic.snareProb) {
            // House uses claps instead of snares
            if (genMusic.style == STYLE_HOUSE) {
                drumClap();
            } else {
                if (rndFloat() < 0.15f) drumClap(); else drumSnare();
            }
        }
        if ((dp & 4) && rndFloat() < genMusic.hhProb) {
            drumClosedHH();
        }
        if ((dp & 8) && rndFloat() < genMusic.hhProb) {
            drumOpenHH();
        }
        
        // Random percussion hits (less in house, more sparse)
        if (genMusic.style != STYLE_HOUSE) {
            if (rndFloat() < 0.03f) drumRimshot();
            if (rndFloat() < 0.02f) drumCowbell();
        }
        
        // === BASS ===
        unsigned char bp = genMusic.bassPattern[tick];
        if (bp > 0 && rndFloat() < genMusic.bassProb) {
            int degree = genMusic.chordRoot;
            int accent = bp;  // Use pattern value as accent level
            
            // For house, add melodic movement to the 303 line
            if (genMusic.style == STYLE_HOUSE) {
                // Move around the scale more
                if (rndFloat() < 0.4f) {
                    degree += rndInt(-3, 3);
                }
            } else {
                // Other styles: occasional variation
                if (rndFloat() < 0.2f) degree += rndInt(-1, 1);
            }
            
            playBassNote(degree, accent);
        }
        
        // === MELODY ===
        bool playMelody = false;
        if (genMusic.style == STYLE_HOUSE) {
            // House: sparse stabs on offbeats
            playMelody = (tick == 2 || tick == 10) && rndFloat() < genMusic.melodyProb;
        } else if (genMusic.style == STYLE_HIPHOP) {
            // Hip-hop: Rhodes-style chords, sparse
            playMelody = (tick == 0 || tick == 8) && rndFloat() < genMusic.melodyProb;
        } else {
            // Claudish: original timing
            playMelody = (tick % 4 == 0 || tick % 4 == 3) && rndFloat() < genMusic.melodyProb;
        }
        
        if (playMelody) {
            int step = rndInt(-2, 2);
            if (rndFloat() < 0.15f) step = rndInt(-5, 5);
            int newNote = genMusic.lastMelodyNote + step;
            newNote = (newNote < 0) ? 0 : (newNote > 14) ? 14 : newNote;
            playMelodyNote(newNote);
        }
        
        // === VOICE ===
        if (tick == 0 && rndFloat() < genMusic.voiceProb) {
            int degree = genMusic.chordRoot + rndInt(0, 4);
            playVoxNote(degree);
        }
        
        // Advance tick
        genMusic.tick = (tick + 1) % 16;
        
        // New bar
        if (genMusic.tick == 0) {
            genMusic.bar++;
            
            // Chord changes
            if (genMusic.bar % 4 == 0) {
                if (genMusic.style == STYLE_HOUSE) {
                    // House: often stays on one chord, or simple changes
                    if (rndFloat() < 0.3f) {
                        int chordOptions[] = {0, 5};  // i or vi
                        genMusic.chordRoot = chordOptions[rndInt(0, 1)];
                    }
                } else {
                    int chordOptions[] = {0, 3, 4, 5};
                    genMusic.chordRoot = chordOptions[rndInt(0, 3)];
                }
            }
            
            // Regenerate patterns
            if (genMusic.bar % 8 == 0) {
                generateDrumPattern();
                generateBassPattern();
                genMusic.patternVariation++;
            }
            
            // Release sustained notes
            if (rndFloat() < 0.3f && genMusic.melodyVoice >= 0) {
                releaseNote(genMusic.melodyVoice);
                genMusic.melodyVoice = -1;
            }
            if (rndFloat() < 0.5f && genMusic.voxVoice >= 0) {
                releaseNote(genMusic.voxVoice);
                genMusic.voxVoice = -1;
            }
        }
    }
}

// Toggle generative music on/off
static void toggleGenMusic(void) {
    genMusic.active = !genMusic.active;
    if (genMusic.active) {
        initGenMusic();
        generateDrumPattern();
        generateBassPattern();
        genMusic.tick = 0;
        genMusic.bar = 0;
        genMusic.timer = 0.0f;
    } else {
        // Release any held notes
        if (genMusic.bassVoice >= 0) releaseNote(genMusic.bassVoice);
        if (genMusic.melodyVoice >= 0) releaseNote(genMusic.melodyVoice);
        if (genMusic.voxVoice >= 0) releaseNote(genMusic.voxVoice);
        genMusic.bassVoice = -1;
        genMusic.melodyVoice = -1;
        genMusic.voxVoice = -1;
    }
}

#endif // GENMUSIC_H
