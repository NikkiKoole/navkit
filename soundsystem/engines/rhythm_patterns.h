// PixelSynth - Rhythm Pattern Generator
// Classic organ-style rhythm patterns: Rock, Pop, Bossa Nova, Cha-Cha, Swing, etc.
// Generates musically interesting drum patterns

#ifndef PIXELSYNTH_RHYTHM_PATTERNS_H
#define PIXELSYNTH_RHYTHM_PATTERNS_H

#include <stdbool.h>
#include <string.h>

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
// Velocity values: 0 = off, 0.1-1.0 = on with velocity
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
        .kick   = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .hihat  = {0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // POP - Steady 4-on-floor with snare on 2 & 4
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .hihat  = {0.7, 0.5, 0.7, 0.5, 0.7, 0.5, 0.7, 0.5, 0.7, 0.5, 0.7, 0.5, 0.7, 0.5, 0.7, 0.5},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 0, .recommendedBpm = 110
    },
    
    // DISCO - Four-on-floor kick, open hihat on off-beats
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .hihat  = {0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5},
        .perc   = {0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.8, 0.0},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // FUNK - Syncopated kick, tight snare
    {
        .kick   = {1.0, 0.0, 0.0, 0.5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 1.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.3},
        .hihat  = {0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5, 0.9, 0.5},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 4, .recommendedBpm = 100
    },
    
    // BOSSA NOVA - Classic Brazilian rhythm
    {
        .kick   = {1.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0},
        .hihat  = {0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3},
        .perc   = {0.8, 0.0, 0.0, 0.6, 0.0, 0.0, 0.8, 0.0, 0.0, 0.6, 0.0, 0.0, 0.8, 0.0, 0.0, 0.6},
        .length = 16, .swingAmount = 3, .recommendedBpm = 130
    },
    
    // CHA-CHA - Classic Latin cha-cha rhythm
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .hihat  = {0.7, 0.4, 0.7, 0.4, 0.7, 0.4, 0.7, 0.4, 0.7, 0.4, 0.7, 0.4, 0.7, 0.4, 0.7, 0.4},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.9, 0.0, 0.7, 0.7, 0.0, 0.0, 0.0, 0.0, 0.9, 0.0, 0.7, 0.7},
        .length = 16, .swingAmount = 0, .recommendedBpm = 120
    },
    
    // SWING - Jazz swing feel
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.9, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.9, 0.0, 0.0, 0.5},
        .hihat  = {0.9, 0.0, 0.5, 0.0, 0.9, 0.0, 0.5, 0.0, 0.9, 0.0, 0.5, 0.0, 0.9, 0.0, 0.5, 0.0},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 8, .recommendedBpm = 140
    },
    
    // FOXTROT - Smooth ballroom dance rhythm
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0},
        .hihat  = {0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3, 0.6, 0.3, 0.5, 0.3},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 4, .recommendedBpm = 110
    },
    
    // REGGAE - Off-beat emphasis (one-drop)
    {
        .kick   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .hihat  = {0.5, 0.0, 0.8, 0.0, 0.5, 0.0, 0.8, 0.0, 0.5, 0.0, 0.8, 0.0, 0.5, 0.0, 0.8, 0.0},
        .perc   = {0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.7, 0.0},
        .length = 16, .swingAmount = 2, .recommendedBpm = 80
    },
    
    // HIP HOP - Boom bap style
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.4},
        .hihat  = {0.8, 0.0, 0.5, 0.0, 0.8, 0.0, 0.5, 0.0, 0.8, 0.0, 0.5, 0.0, 0.8, 0.0, 0.5, 0.5},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 6, .recommendedBpm = 90
    },
    
    // HOUSE - Four-on-floor electronic
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .hihat  = {0.9, 0.0, 0.9, 0.0, 0.9, 0.0, 0.9, 0.0, 0.9, 0.0, 0.9, 0.0, 0.9, 0.0, 0.9, 0.0},
        .perc   = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .length = 16, .swingAmount = 0, .recommendedBpm = 124
    },
    
    // LATIN - Tumbao-inspired pattern
    {
        .kick   = {1.0, 0.0, 0.0, 0.6, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.0, 0.0, 1.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0, 0.0, 0.0},
        .hihat  = {0.8, 0.4, 0.8, 0.4, 0.8, 0.4, 0.8, 0.4, 0.8, 0.4, 0.8, 0.4, 0.8, 0.4, 0.8, 0.4},
        .perc   = {0.8, 0.0, 0.0, 0.5, 0.0, 0.5, 0.8, 0.0, 0.0, 0.5, 0.0, 0.0, 0.8, 0.0, 0.0, 0.5},
        .length = 16, .swingAmount = 0, .recommendedBpm = 100
    },
    
    // WALTZ - 3/4 time
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .hihat  = {0.8, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .length = 12, .swingAmount = 0, .recommendedBpm = 140
    },
    
    // SHUFFLE - Blues shuffle feel
    {
        .kick   = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .snare  = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
        .hihat  = {0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0, 0.9, 0.0, 0.6, 0.0},
        .perc   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
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

typedef struct {
    RhythmStyle style;
    RhythmVariation variation;
    unsigned int noiseState;
    float intensity;         // 0.0-1.0, affects velocity variation
    float humanize;          // 0.0-1.0, adds velocity randomization
} RhythmGenerator;

static void initRhythmGenerator(RhythmGenerator* gen) {
    gen->style = RHYTHM_ROCK;
    gen->variation = RHYTHM_VAR_NONE;
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
            p->drumSteps[track][step] = false;
            p->drumVelocity[track][step] = 0.8f;
            p->drumPitch[track][step] = 0.0f;
        }
        p->drumTrackLength[track] = src->length;
    }
    
    // Apply base pattern
    for (int step = 0; step < src->length; step++) {
        // Kick (track 0)
        if (src->kick[step] > 0.0f) {
            float vel = src->kick[step] * gen->intensity;
            if (gen->humanize > 0.0f) {
                vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
            }
            if (vel > 0.1f) {
                p->drumSteps[0][step] = true;
                p->drumVelocity[0][step] = vel < 1.0f ? vel : 1.0f;
            }
        }
        
        // Snare (track 1)
        if (src->snare[step] > 0.0f) {
            float vel = src->snare[step] * gen->intensity;
            if (gen->humanize > 0.0f) {
                vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
            }
            if (vel > 0.1f) {
                p->drumSteps[1][step] = true;
                p->drumVelocity[1][step] = vel < 1.0f ? vel : 1.0f;
            }
        }
        
        // HiHat (track 2)
        if (src->hihat[step] > 0.0f) {
            float vel = src->hihat[step] * gen->intensity;
            if (gen->humanize > 0.0f) {
                vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
            }
            if (vel > 0.1f) {
                p->drumSteps[2][step] = true;
                p->drumVelocity[2][step] = vel < 1.0f ? vel : 1.0f;
            }
        }
        
        // Percussion (track 3)
        if (src->perc[step] > 0.0f) {
            float vel = src->perc[step] * gen->intensity;
            if (gen->humanize > 0.0f) {
                vel += (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.3f;
            }
            if (vel > 0.1f) {
                p->drumSteps[3][step] = true;
                p->drumVelocity[3][step] = vel < 1.0f ? vel : 1.0f;
            }
        }
    }
    
    // Apply variations
    switch (gen->variation) {
        case RHYTHM_VAR_FILL:
            // Add fills at end of pattern (last 4 steps)
            for (int step = src->length - 4; step < src->length; step++) {
                if (rhythmRandFloat(gen) < 0.5f && !p->drumSteps[1][step]) {
                    p->drumSteps[1][step] = true;
                    p->drumVelocity[1][step] = 0.6f + rhythmRandFloat(gen) * 0.4f;
                }
            }
            break;
            
        case RHYTHM_VAR_SPARSE:
            // Remove some hits (especially ghost notes and weak beats)
            for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
                for (int step = 0; step < src->length; step++) {
                    if (p->drumSteps[track][step] && p->drumVelocity[track][step] < 0.7f) {
                        if (rhythmRandFloat(gen) < 0.5f) {
                            p->drumSteps[track][step] = false;
                        }
                    }
                }
            }
            break;
            
        case RHYTHM_VAR_BUSY:
            // Add ghost notes
            for (int step = 0; step < src->length; step++) {
                // Add ghost snares
                if (!p->drumSteps[1][step] && rhythmRandFloat(gen) < 0.2f) {
                    p->drumSteps[1][step] = true;
                    p->drumVelocity[1][step] = 0.3f + rhythmRandFloat(gen) * 0.2f;
                }
                // Add extra hihats
                if (!p->drumSteps[2][step] && rhythmRandFloat(gen) < 0.4f) {
                    p->drumSteps[2][step] = true;
                    p->drumVelocity[2][step] = 0.3f + rhythmRandFloat(gen) * 0.2f;
                }
            }
            break;
            
        case RHYTHM_VAR_SYNCOPATED:
            // Shift some kick hits by one step
            for (int step = 0; step < src->length - 1; step++) {
                if (p->drumSteps[0][step] && !p->drumSteps[0][step + 1] && rhythmRandFloat(gen) < 0.3f) {
                    p->drumSteps[0][step] = false;
                    p->drumSteps[0][step + 1] = true;
                    p->drumVelocity[0][step + 1] = p->drumVelocity[0][step];
                }
            }
            break;
            
        default:
            break;
    }
}

// Get recommended swing for current style
static int getRhythmSwing(RhythmGenerator* gen) {
    return rhythmPatterns[gen->style].swingAmount;
}

// Get recommended BPM for current style
static int getRhythmRecommendedBpm(RhythmGenerator* gen) {
    return rhythmPatterns[gen->style].recommendedBpm;
}

#endif // PIXELSYNTH_RHYTHM_PATTERNS_H
