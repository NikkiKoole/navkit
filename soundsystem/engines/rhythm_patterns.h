// PixelSynth - Rhythm Pattern Generator
// Classic organ-style rhythm patterns: Rock, Pop, Bossa Nova, Cha-Cha, Swing, etc.
// Generates musically interesting drum patterns

#ifndef PIXELSYNTH_RHYTHM_PATTERNS_H
#define PIXELSYNTH_RHYTHM_PATTERNS_H

#include <stdbool.h>
#include <string.h>
#include "rhythm_prob_maps.h"

// ============================================================================
// RHYTHM STYLE DEFINITIONS
// ============================================================================

typedef enum {
    RHYTHM_ROCK = 0,
    RHYTHM_POP,
    RHYTHM_DISCO,
    RHYTHM_FUNK,
    RHYTHM_BOSSA_NOVA,
    RHYTHM_CHA_CHA,
    RHYTHM_SWING,
    RHYTHM_FOXTROT,
    RHYTHM_REGGAE,
    RHYTHM_HIP_HOP,
    RHYTHM_HOUSE,
    RHYTHM_LATIN,
    RHYTHM_WALTZ,
    RHYTHM_SHUFFLE,
    RHYTHM_COUNT
} RhythmStyle;

static const char* rhythmStyleNames[RHYTHM_COUNT] = {
    "Rock",
    "Pop", 
    "Disco",
    "Funk",
    "Bossa Nova",
    "Cha-Cha",
    "Swing",
    "Foxtrot",
    "Reggae",
    "Hip Hop",
    "House",
    "Latin",
    "Waltz",
    "Shuffle"
};

// ============================================================================
// RHYTHM PATTERN DATA
// ============================================================================

// Pattern data: 16 steps per track
// Velocity values: 0 = off, 0.1-1.0f = on with velocity
// Each pattern defines: kick, snare, hihat, percussion

typedef struct {
    float kick[16];
    float snare[16];
    float hihat[16];
    float perc[16];      // Percussion (clap, cowbell, etc)
    int length;          // Pattern length (can be < 16 for time signatures)
    int swingAmount;     // Suggested swing (0-12)
    int recommendedBpm;  // Suggested tempo
} RhythmPatternData;

// ============================================================================
// PATTERN DEFINITIONS
// ============================================================================

static const RhythmPatternData rhythmPatterns[RHYTHM_COUNT] = {
    // ROCK - Classic rock beat, kick on 1 & 3, snare on 2 & 4
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // POP - Steady 4-on-floor with snare on 2 & 4
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.5f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 110
    },
    
    // DISCO - Four-on-floor kick, open hihat on off-beats
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f},
        .perc   = {0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // FUNK - Syncopated kick, tight snare
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.3f},
        .hihat  = {0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f, 0.9f, 0.5f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 4, .recommendedBpm = 100
    },
    
    // BOSSA NOVA - Classic Brazilian rhythm
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f},
        .hihat  = {0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f},
        .perc   = {0.8f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.6f},
        .length = 16, .swingAmount = 3, .recommendedBpm = 130
    },
    
    // CHA-CHA - Classic Latin cha-cha rhythm
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f, 0.7f, 0.4f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.7f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.7f, 0.7f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // SWING - Jazz swing feel
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.5f},
        .hihat  = {0.9f, 0.0f, 0.5f, 0.0f, 0.9f, 0.0f, 0.5f, 0.0f, 0.9f, 0.0f, 0.5f, 0.0f, 0.9f, 0.0f, 0.5f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 8, .recommendedBpm = 140
    },
    
    // FOXTROT - Smooth ballroom dance rhythm
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f, 0.6f, 0.3f, 0.5f, 0.3f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 4, .recommendedBpm = 110
    },
    
    // REGGAE - Off-beat emphasis (one-drop)
    {
        .kick   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f},
        .length = 16, .swingAmount = 2, .recommendedBpm = 80
    },
    
    // HIP HOP - Boom bap style
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.4f},
        .hihat  = {0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.5f, 0.5f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 6, .recommendedBpm = 90
    },
    
    // HOUSE - Four-on-floor electronic
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f, 0.9f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 124
    },
    
    // LATIN - Tumbao-inspired pattern
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 1.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f, 0.8f, 0.4f},
        .perc   = {0.8f, 0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.8f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.5f},
        .length = 16, .swingAmount = 0, .recommendedBpm = 100
    },
    
    // WALTZ - 3/4 time
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.8f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 12, .swingAmount = 0, .recommendedBpm = 140
    },
    
    // SHUFFLE - Blues shuffle feel
    {
        .kick   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .snare  = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        .hihat  = {0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 0.6f, 0.0f},
        .perc   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        .length = 16, .swingAmount = 10, .recommendedBpm = 120
    }
};

// ============================================================================
// VARIATION TYPES
// ============================================================================

typedef enum {
    RHYTHM_VAR_NONE = 0,      // No variation
    RHYTHM_VAR_FILL,          // Add fills
    RHYTHM_VAR_SPARSE,        // Remove some hits
    RHYTHM_VAR_BUSY,          // Add ghost notes
    RHYTHM_VAR_SYNCOPATED,    // Shift some hits
    RHYTHM_VAR_COUNT
} RhythmVariation;

static const char* rhythmVariationNames[RHYTHM_VAR_COUNT] = {
    "Normal",
    "Fill",
    "Sparse",
    "Busy",
    "Synco"
};

// ============================================================================
// RHYTHM GENERATOR CONTEXT
// ============================================================================

typedef enum {
    RHYTHM_MODE_CLASSIC = 0,  // Fixed patterns + variation modes
    RHYTHM_MODE_PROB_MAP,     // Probability maps + density knob
    RHYTHM_MODE_COUNT
} RhythmMode;

typedef struct {
    RhythmMode mode;
    // Classic mode
    RhythmStyle style;
    RhythmVariation variation;
    // Prob map mode
    int probStyle;           // ProbMapStyle index (int to avoid cross-header enum dep)
    float density;           // 0.0-1.0, threshold for probability maps
    float randomize;         // 0.0-1.0, per-generation jitter
    // Shared
    unsigned int noiseState;
    float intensity;         // 0.0-1.0, affects velocity variation
    float humanize;          // 0.0-1.0, adds velocity randomization
} RhythmGenerator;

static void initRhythmGenerator(RhythmGenerator* gen) {
    gen->mode = RHYTHM_MODE_CLASSIC;
    gen->style = RHYTHM_ROCK;
    gen->variation = RHYTHM_VAR_NONE;
    gen->probStyle = 16; // PROB_ROCK
    gen->density = 0.55f;
    gen->randomize = 0.0f;
    gen->noiseState = 54321;
    gen->intensity = 0.8f;
    gen->humanize = 0.1f;
}

// Random float helper for generator
static float rhythmRandFloat(RhythmGenerator* gen) {
    gen->noiseState = gen->noiseState * 1103515245 + 12345;
    return (float)(gen->noiseState >> 16) / 65535.0f;
}

// ============================================================================
// PATTERN GENERATION
// ============================================================================

// Apply rhythm pattern to sequencer pattern (drum tracks only)
// p: target Pattern (sequencer pattern)
// gen: rhythm generator settings
static void applyRhythmPattern(Pattern* p, RhythmGenerator* gen) {
    if (!p || !gen) return;
    
    const RhythmPatternData* src = &rhythmPatterns[gen->style];
    
    // Clear existing drum steps
    for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
        for (int step = 0; step < SEQ_MAX_STEPS; step++) {
            patClearDrum(p, step);
        }
        patSetDrumLength(p, src->length);
    }

    // Apply base pattern
    const float *srcTracks[4] = { src->kick, src->snare, src->hihat, src->perc };
    for (int step = 0; step < src->length; step++) {
        for (int track = 0; track < 4; track++) {
            if (srcTracks[track][step] > 0.0f) {
                float vel = srcTracks[track][step] * gen->intensity;
                if (gen->humanize > 0.0f) {
                    vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
                }
                if (vel > 0.1f) {
                    patSetDrum(p, step, vel < 1.0f ? vel : 1.0f, 0.0f);
                }
            }
        }
    }
    
    // Apply variations
    switch (gen->variation) {
        case RHYTHM_VAR_FILL:
            // Add fills at end of pattern (last 4 steps)
            for (int step = src->length - 4; step < src->length; step++) {
                if (rhythmRandFloat(gen) < 0.5f && !patGetDrum(p, step)) {
                    patSetDrum(p, step, 0.6f + rhythmRandFloat(gen) * 0.4f, 0.0f);
                }
            }
            break;

        case RHYTHM_VAR_SPARSE:
            // Remove some hits (especially ghost notes and weak beats)
            for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
                for (int step = 0; step < src->length; step++) {
                    if (patGetDrum(p, step) && patGetDrumVel(p, step) < 0.7f) {
                        if (rhythmRandFloat(gen) < 0.5f) {
                            patClearDrum(p, step);
                        }
                    }
                }
            }
            break;

        case RHYTHM_VAR_BUSY:
            // Add ghost notes
            for (int step = 0; step < src->length; step++) {
                // Add ghost snares
                if (!patGetDrum(p, step) && rhythmRandFloat(gen) < 0.2f) {
                    patSetDrum(p, step, 0.3f + rhythmRandFloat(gen) * 0.2f, 0.0f);
                }
                // Add extra hihats
                if (!patGetDrum(p, step) && rhythmRandFloat(gen) < 0.4f) {
                    patSetDrum(p, step, 0.3f + rhythmRandFloat(gen) * 0.2f, 0.0f);
                }
            }
            break;

        case RHYTHM_VAR_SYNCOPATED:
            // Move on-beat hits to preceding off-beat (anticipation) across all tracks
            for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
                for (int step = 0; step < src->length; step++) {
                    if (patGetDrum(p, step) && (step % 4 == 0) && step > 0 && !patGetDrum(p, step-1)) {
                        if (rhythmRandFloat(gen) < 0.3f) {
                            float vel = patGetDrumVel(p, step);
                            patClearDrum(p, step);
                            patSetDrum(p, step-1, vel * 0.85f, 0.0f);
                        }
                    }
                }
            }
            break;
            
        default:
            break;
    }
}

// ============================================================================
// PROBABILITY MAP GENERATION (Grids-style)
// ============================================================================

// Apply probability map to sequencer pattern (drum tracks only)
// Density acts as threshold: low = skeleton beat, high = full groove
static void applyRhythmProbMap(Pattern* p, RhythmGenerator* gen) {
    if (!p || !gen) return;
    int style = gen->probStyle;
    if (style < 0 || style >= PROB_MAP_COUNT) style = 0;
    const RhythmProbMap *map = &probMaps[style];

    // Threshold: density 0.0 → 255 (nothing fires), density 1.0 → 0 (everything fires)
    int threshold = (int)((1.0f - gen->density) * 255.0f);

    // Clear existing drum steps and set track lengths
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) patClearDrum(p, s);
        patSetDrumLength(p, map->length);
    }

    const uint8_t *tracks[4] = { map->kick, map->snare, map->hihat, map->perc };

    for (int s = 0; s < map->length; s++) {
        for (int t = 0; t < 4; t++) {
            int prob = tracks[t][s];

            // Optional jitter: randomize shifts probabilities around
            if (gen->randomize > 0.0f) {
                prob += (int)((rhythmRandFloat(gen) - 0.5f) * 40.0f * gen->randomize);
                if (prob < 0) prob = 0;
                if (prob > 255) prob = 255;
            }

            if (prob <= threshold) continue;

            // Velocity scales with how far above threshold (essential hits = louder)
            float baseVel = 0.4f + 0.6f * ((float)(prob - threshold) / (float)(255 - threshold));

            // Accent boost
            float accent = (map->accent[s] > threshold) ? 0.15f : 0.0f;

            // Humanize
            float humanize = (gen->humanize > 0.0f)
                ? (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.2f
                : 0.0f;

            float vel = fminf(1.0f, fmaxf(0.1f, (baseVel + accent + humanize) * gen->intensity));
            patSetDrum(p, s, vel, 0.0f);
        }
    }
}

// ============================================================================
// UNIFIED HELPERS (work with both modes)
// ============================================================================

// Get recommended swing for current mode/style
static int getRhythmSwing(RhythmGenerator* gen) {
    if (gen->mode == RHYTHM_MODE_PROB_MAP) {
        int s = gen->probStyle;
        if (s < 0 || s >= PROB_MAP_COUNT) s = 0;
        return probMaps[s].swingAmount;
    }
    return rhythmPatterns[gen->style].swingAmount;
}

// Get recommended BPM for current mode/style
static int getRhythmRecommendedBpm(RhythmGenerator* gen) {
    if (gen->mode == RHYTHM_MODE_PROB_MAP) {
        int s = gen->probStyle;
        if (s < 0 || s >= PROB_MAP_COUNT) s = 0;
        return probMaps[s].recommendedBpm;
    }
    return rhythmPatterns[gen->style].recommendedBpm;
}

// Generate pattern using current mode
static void generateRhythm(Pattern* p, RhythmGenerator* gen) {
    if (gen->mode == RHYTHM_MODE_PROB_MAP) {
        applyRhythmProbMap(p, gen);
    } else {
        applyRhythmPattern(p, gen);
    }
}

// Apply rhythm writing each drum track to its own dedicated single-track pattern.
// trackPats[i] receives only track i's data. Use with perTrackPatterns mode.
// nTracks capped at SEQ_DRUM_TRACKS (4).
static void applyRhythmPerTrack(Pattern **trackPats, int nTracks, RhythmGenerator *gen) {
    if (!trackPats || !gen) return;
    if (nTracks < 1) return;
    if (nTracks > SEQ_DRUM_TRACKS) nTracks = SEQ_DRUM_TRACKS;

    const RhythmPatternData *src = &rhythmPatterns[gen->style];
    const float *srcTracks[4] = { src->kick, src->snare, src->hihat, src->perc };

    for (int t = 0; t < nTracks; t++) {
        if (!trackPats[t]) continue;
        for (int s = 0; s < SEQ_MAX_STEPS; s++) patClearDrum(trackPats[t], s);
        patSetDrumLength(trackPats[t], src->length);
    }

    for (int step = 0; step < src->length; step++) {
        for (int t = 0; t < nTracks; t++) {
            if (!trackPats[t]) continue;
            if (srcTracks[t][step] > 0.0f) {
                float vel = srcTracks[t][step] * gen->intensity;
                if (gen->humanize > 0.0f) {
                    vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
                }
                if (vel > 0.1f) {
                    patSetDrum(trackPats[t], step, vel < 1.0f ? vel : 1.0f, 0.0f);
                }
            }
        }
    }

    switch (gen->variation) {
        case RHYTHM_VAR_FILL:
            if (nTracks > 1 && trackPats[1]) {
                for (int step = src->length - 4; step < src->length; step++) {
                    if (rhythmRandFloat(gen) < 0.5f && !patGetDrum(trackPats[1], step)) {
                        patSetDrum(trackPats[1], step, 0.6f + rhythmRandFloat(gen) * 0.4f, 0.0f);
                    }
                }
            }
            break;
        case RHYTHM_VAR_SPARSE:
            for (int t = 0; t < nTracks; t++) {
                if (!trackPats[t]) continue;
                for (int step = 0; step < src->length; step++) {
                    if (patGetDrum(trackPats[t], step) && patGetDrumVel(trackPats[t], step) < 0.7f) {
                        if (rhythmRandFloat(gen) < 0.5f) patClearDrum(trackPats[t], step);
                    }
                }
            }
            break;
        case RHYTHM_VAR_BUSY:
            for (int t = 1; t < nTracks && t < 3; t++) {
                if (!trackPats[t]) continue;
                float addProb = (t == 1) ? 0.2f : 0.4f;
                for (int step = 0; step < src->length; step++) {
                    if (!patGetDrum(trackPats[t], step) && rhythmRandFloat(gen) < addProb) {
                        patSetDrum(trackPats[t], step, 0.3f + rhythmRandFloat(gen) * 0.2f, 0.0f);
                    }
                }
            }
            break;
        case RHYTHM_VAR_SYNCOPATED:
            for (int t = 0; t < nTracks; t++) {
                if (!trackPats[t]) continue;
                for (int step = 0; step < src->length; step++) {
                    if (patGetDrum(trackPats[t], step) && (step % 4 == 0) && step > 0 && !patGetDrum(trackPats[t], step-1)) {
                        if (rhythmRandFloat(gen) < 0.3f) {
                            float vel = patGetDrumVel(trackPats[t], step);
                            patClearDrum(trackPats[t], step);
                            patSetDrum(trackPats[t], step-1, vel * 0.85f, 0.0f);
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// EUCLIDEAN RHYTHM GENERATOR (Bjorklund's algorithm)
// ============================================================================

// Distribute `hits` evenly across `steps`, rotated by `rotation`
static void euclidean(bool *out, int steps, int hits, int rotation) {
    if (steps <= 0 || steps > SEQ_MAX_STEPS) return;
    if (hits < 0) hits = 0;
    if (hits > steps) hits = steps;
    int bucket = 0;
    for (int i = 0; i < steps; i++) {
        bucket += hits;
        if (bucket >= steps) {
            bucket -= steps;
            out[(i + rotation) % steps] = true;
        } else {
            out[(i + rotation) % steps] = false;
        }
    }
}

// Apply euclidean pattern to a single drum track
static void applyEuclideanToTrack(Pattern* p, int track, int hits, int steps, int rotation, float vel) {
    if (!p || track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (steps < 1) steps = 1;
    if (steps > SEQ_MAX_STEPS) steps = SEQ_MAX_STEPS;
    bool pattern[SEQ_MAX_STEPS] = {false};
    euclidean(pattern, steps, hits, rotation);
    for (int s = 0; s < SEQ_MAX_STEPS; s++) patClearDrum(p, s);
    patSetDrumLength(p, steps);
    for (int s = 0; s < steps; s++) {
        if (pattern[s]) patSetDrum(p, s, vel, 0.0f);
    }
}

#endif // PIXELSYNTH_RHYTHM_PATTERNS_H
