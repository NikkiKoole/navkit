// PixelSynth - Sample Playback Engine
// Load and play WAV samples with pitch/volume control

#ifndef PIXELSYNTH_SAMPLER_H
#define PIXELSYNTH_SAMPLER_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define SAMPLER_MAX_SAMPLES 32       // Maximum loaded samples
#define SAMPLER_MAX_VOICES 8         // Polyphony for sample playback
#define SAMPLER_MAX_SAMPLE_LENGTH 262144  // ~5.4 seconds at 48kHz

// ============================================================================
// TYPES
// ============================================================================

// Loaded sample data
typedef struct {
    float* data;              // Sample data (normalized -1 to 1)
    int length;               // Number of samples
    int sampleRate;           // Original sample rate
    bool loaded;              // Whether this slot has a sample
    bool embedded;            // True if data points to embedded const data (don't free)
    char name[64];            // Sample name (for display)
} Sample;

// Sample playback voice
typedef struct {
    bool active;
    int sampleIndex;          // Which sample is playing
    float position;           // Playback position (fractional for pitch shift)
    float speed;              // Playback speed (1.0 = normal, 2.0 = octave up)
    float volume;             // Voice volume (0-1)
    float pan;                // Pan (-1 left, 0 center, 1 right)
    bool loop;                // Loop the sample
    int loopStart;            // Loop start point
    int loopEnd;              // Loop end point
} SamplerVoice;

// ============================================================================
// SAMPLER CONTEXT
// ============================================================================

typedef struct SamplerContext {
    Sample samples[SAMPLER_MAX_SAMPLES];
    SamplerVoice voices[SAMPLER_MAX_VOICES];
    float volume;             // Master volume
    int sampleRate;           // Output sample rate (for resampling)
} SamplerContext;

// ============================================================================
// GLOBAL CONTEXT
// ============================================================================

static SamplerContext _samplerCtx;
static SamplerContext* samplerCtx = &_samplerCtx;
static bool _samplerCtxInitialized = false;

static void initSamplerContext(SamplerContext* ctx) {
    memset(ctx, 0, sizeof(SamplerContext));
    ctx->volume = 1.0f;
    ctx->sampleRate = 48000;  // Default to 48kHz
}

static void _ensureSamplerCtx(void) {
    if (!_samplerCtxInitialized) {
        initSamplerContext(samplerCtx);
        _samplerCtxInitialized = true;
    }
}

// Backward-compatible macros
#define samplerSamples (samplerCtx->samples)
#define samplerVoices (samplerCtx->voices)
#define samplerVolume (samplerCtx->volume)

// ============================================================================
// WAV FILE LOADING
// ============================================================================

// WAV file header structures
#pragma pack(push, 1)
typedef struct {
    char riff[4];             // "RIFF"
    uint32_t fileSize;        // File size - 8
    char wave[4];             // "WAVE"
} WavHeader;

typedef struct {
    char id[4];               // Chunk ID
    uint32_t size;            // Chunk size
} WavChunk;

typedef struct {
    uint16_t audioFormat;     // 1 = PCM, 3 = IEEE float
    uint16_t numChannels;     // 1 = mono, 2 = stereo
    uint32_t sampleRate;      // Sample rate
    uint32_t byteRate;        // Bytes per second
    uint16_t blockAlign;      // Bytes per sample frame
    uint16_t bitsPerSample;   // Bits per sample (8, 16, 24, 32)
} WavFmt;
#pragma pack(pop)

// Load a WAV file into a sample slot
// Returns sample index on success, -1 on failure
static int samplerLoadWav(const char* filepath, int slotIndex) {
    _ensureSamplerCtx();
    
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) {
        return -1;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }
    
    // Read RIFF header
    WavHeader header;
    if (fread(&header, sizeof(WavHeader), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    // Validate RIFF/WAVE
    if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
        fclose(file);
        return -1;
    }
    
    // Find fmt and data chunks
    WavFmt fmt = {0};
    uint32_t dataSize = 0;
    long dataOffset = 0;
    bool foundFmt = false;
    bool foundData = false;
    
    while (!foundFmt || !foundData) {
        WavChunk chunk;
        if (fread(&chunk, sizeof(WavChunk), 1, file) != 1) {
            break;
        }
        
        if (memcmp(chunk.id, "fmt ", 4) == 0) {
            if (fread(&fmt, sizeof(WavFmt), 1, file) != 1) {
                fclose(file);
                return -1;
            }
            // Skip any extra format bytes
            if (chunk.size > sizeof(WavFmt)) {
                fseek(file, chunk.size - sizeof(WavFmt), SEEK_CUR);
            }
            foundFmt = true;
        } else if (memcmp(chunk.id, "data", 4) == 0) {
            dataSize = chunk.size;
            dataOffset = ftell(file);
            foundData = true;
            break;  // Found data, stop searching
        } else {
            // Skip unknown chunk
            fseek(file, chunk.size, SEEK_CUR);
        }
    }
    
    if (!foundFmt || !foundData) {
        fclose(file);
        return -1;
    }
    
    // Only support PCM format (1) or IEEE float (3)
    if (fmt.audioFormat != 1 && fmt.audioFormat != 3) {
        fclose(file);
        return -1;
    }
    
    // Calculate number of samples
    int bytesPerSample = fmt.bitsPerSample / 8;
    int numSamples = dataSize / (bytesPerSample * fmt.numChannels);
    
    if (numSamples > SAMPLER_MAX_SAMPLE_LENGTH) {
        numSamples = SAMPLER_MAX_SAMPLE_LENGTH;
    }
    
    // Free existing sample if any
    Sample* sample = &samplerCtx->samples[slotIndex];
    if (sample->data) {
        free(sample->data);
        sample->data = NULL;
    }
    
    // Allocate sample buffer
    sample->data = (float*)malloc(numSamples * sizeof(float));
    if (!sample->data) {
        fclose(file);
        return -1;
    }
    
    // Read and convert sample data
    fseek(file, dataOffset, SEEK_SET);
    
    for (int i = 0; i < numSamples; i++) {
        float value = 0.0f;
        
        if (fmt.audioFormat == 1) {  // PCM
            if (fmt.bitsPerSample == 8) {
                uint8_t val;
                if (fread(&val, 1, 1, file) == 1) {
                    value = (val - 128) / 128.0f;
                }
            } else if (fmt.bitsPerSample == 16) {
                int16_t val;
                if (fread(&val, 2, 1, file) == 1) {
                    value = val / 32768.0f;
                }
            } else if (fmt.bitsPerSample == 24) {
                uint8_t bytes[3];
                if (fread(bytes, 3, 1, file) == 1) {
                    // Convert 24-bit to 32-bit signed
                    int32_t val = (bytes[2] << 24) | (bytes[1] << 16) | (bytes[0] << 8);
                    value = val / 2147483648.0f;
                }
            } else if (fmt.bitsPerSample == 32) {
                int32_t val;
                if (fread(&val, 4, 1, file) == 1) {
                    value = val / 2147483648.0f;
                }
            }
        } else if (fmt.audioFormat == 3) {  // IEEE float
            if (fmt.bitsPerSample == 32) {
                float val;
                if (fread(&val, 4, 1, file) == 1) {
                    value = val;
                }
            }
        }
        
        // If stereo, skip right channel (or average)
        if (fmt.numChannels == 2) {
            float right = 0.0f;
            // Read and discard/average right channel
            if (fmt.audioFormat == 1 && fmt.bitsPerSample == 16) {
                int16_t val;
                if (fread(&val, 2, 1, file) == 1) {
                    right = val / 32768.0f;
                }
            } else if (fmt.audioFormat == 1 && fmt.bitsPerSample == 24) {
                uint8_t bytes[3];
                if (fread(bytes, 3, 1, file) == 1) {
                    int32_t val = (bytes[2] << 24) | (bytes[1] << 16) | (bytes[0] << 8);
                    right = val / 2147483648.0f;
                }
            } else {
                fseek(file, bytesPerSample, SEEK_CUR);
            }
            // Average left and right
            value = (value + right) * 0.5f;
        }
        
        sample->data[i] = value;
    }
    
    sample->length = numSamples;
    sample->sampleRate = fmt.sampleRate;
    sample->loaded = true;
    sample->embedded = false;  // Loaded from file, can be freed
    
    // Extract filename for display name
    const char* filename = filepath;
    const char* lastSlash = strrchr(filepath, '/');
    if (lastSlash) filename = lastSlash + 1;
    const char* lastBackslash = strrchr(filename, '\\');
    if (lastBackslash) filename = lastBackslash + 1;
    strncpy(sample->name, filename, sizeof(sample->name) - 1);
    sample->name[sizeof(sample->name) - 1] = '\0';
    
    // Remove .wav extension if present
    char* ext = strstr(sample->name, ".wav");
    if (ext) *ext = '\0';
    ext = strstr(sample->name, ".WAV");
    if (ext) *ext = '\0';
    
    fclose(file);
    return slotIndex;
}

// Free a sample slot
static void samplerFreeSample(int slotIndex) {
    _ensureSamplerCtx();
    
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) {
        return;
    }
    
    Sample* sample = &samplerCtx->samples[slotIndex];
    // Only free if not embedded (embedded data is const and shouldn't be freed)
    if (sample->data && !sample->embedded) {
        free(sample->data);
    }
    sample->data = NULL;
    sample->loaded = false;
    sample->embedded = false;
    sample->length = 0;
}

// Free all samples
static void samplerFreeAll(void) {
    _ensureSamplerCtx();
    
    for (int i = 0; i < SAMPLER_MAX_SAMPLES; i++) {
        samplerFreeSample(i);
    }
}

// ============================================================================
// SAMPLE PLAYBACK
// ============================================================================

// Trigger a sample on the next available voice
// pitch: 1.0 = original pitch, 2.0 = octave up, 0.5 = octave down
// Returns voice index or -1 if no voice available
static int samplerPlay(int sampleIndex, float volume, float pitch) {
    _ensureSamplerCtx();
    
    if (sampleIndex < 0 || sampleIndex >= SAMPLER_MAX_SAMPLES) {
        return -1;
    }
    if (!samplerCtx->samples[sampleIndex].loaded) {
        return -1;
    }
    
    // Find free voice
    int voiceIdx = -1;
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (!samplerCtx->voices[i].active) {
            voiceIdx = i;
            break;
        }
    }
    
    // Voice stealing: use oldest voice if none free
    if (voiceIdx < 0) {
        float maxPos = 0;
        for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
            if (samplerCtx->voices[i].position > maxPos) {
                maxPos = samplerCtx->voices[i].position;
                voiceIdx = i;
            }
        }
        if (voiceIdx < 0) voiceIdx = 0;
    }
    
    SamplerVoice* v = &samplerCtx->voices[voiceIdx];
    v->active = true;
    v->sampleIndex = sampleIndex;
    v->position = 0.0f;
    v->volume = volume;
    v->pan = 0.0f;
    v->loop = false;
    
    // Calculate playback speed accounting for sample rate difference
    Sample* sample = &samplerCtx->samples[sampleIndex];
    float rateRatio = (float)sample->sampleRate / (float)samplerCtx->sampleRate;
    v->speed = pitch * rateRatio;
    
    return voiceIdx;
}

// Trigger with pan
static int samplerPlayPanned(int sampleIndex, float volume, float pitch, float pan) {
    int voiceIdx = samplerPlay(sampleIndex, volume, pitch);
    if (voiceIdx >= 0) {
        samplerCtx->voices[voiceIdx].pan = pan;
    }
    return voiceIdx;
}

// Trigger looped playback
static int samplerPlayLooped(int sampleIndex, float volume, float pitch, int loopStart, int loopEnd) {
    int voiceIdx = samplerPlay(sampleIndex, volume, pitch);
    if (voiceIdx >= 0) {
        SamplerVoice* v = &samplerCtx->voices[voiceIdx];
        v->loop = true;
        v->loopStart = loopStart;
        v->loopEnd = loopEnd > 0 ? loopEnd : samplerCtx->samples[sampleIndex].length;
    }
    return voiceIdx;
}

// Stop a specific voice
static void samplerStopVoice(int voiceIndex) {
    _ensureSamplerCtx();
    if (voiceIndex >= 0 && voiceIndex < SAMPLER_MAX_VOICES) {
        samplerCtx->voices[voiceIndex].active = false;
    }
}

// Stop all voices playing a specific sample
static void samplerStopSample(int sampleIndex) {
    _ensureSamplerCtx();
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (samplerCtx->voices[i].active && 
            samplerCtx->voices[i].sampleIndex == sampleIndex) {
            samplerCtx->voices[i].active = false;
        }
    }
}

// Stop all voices
static void samplerStopAll(void) {
    _ensureSamplerCtx();
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        samplerCtx->voices[i].active = false;
    }
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

// Linear interpolation for fractional sample positions
static float samplerInterpolate(float* data, int length, float position) {
    int i0 = (int)position;
    int i1 = i0 + 1;
    
    if (i0 < 0) return 0.0f;
    if (i1 >= length) return data[i0 < length ? i0 : length - 1];
    
    float frac = position - i0;
    return data[i0] * (1.0f - frac) + data[i1] * frac;
}

// Process all sampler voices, returns mono output
static float processSampler(float dt) {
    _ensureSamplerCtx();
    (void)dt;  // dt not needed, we advance by speed
    
    float output = 0.0f;
    
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        SamplerVoice* v = &samplerCtx->voices[i];
        if (!v->active) continue;
        
        Sample* sample = &samplerCtx->samples[v->sampleIndex];
        if (!sample->loaded || !sample->data) {
            v->active = false;
            continue;
        }
        
        // Get interpolated sample value
        float value = samplerInterpolate(sample->data, sample->length, v->position);
        output += value * v->volume;
        
        // Advance position
        v->position += v->speed;
        
        // Handle loop or end
        if (v->loop) {
            int loopEnd = v->loopEnd > 0 ? v->loopEnd : sample->length;
            if (v->position >= loopEnd) {
                v->position = v->loopStart + (v->position - loopEnd);
            }
        } else {
            if (v->position >= sample->length) {
                v->active = false;
            }
        }
    }
    
    return output * samplerCtx->volume;
}

// Process with stereo output (applies panning)
static void processSamplerStereo(float dt, float* left, float* right) {
    _ensureSamplerCtx();
    (void)dt;
    
    float outL = 0.0f;
    float outR = 0.0f;
    
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        SamplerVoice* v = &samplerCtx->voices[i];
        if (!v->active) continue;
        
        Sample* sample = &samplerCtx->samples[v->sampleIndex];
        if (!sample->loaded || !sample->data) {
            v->active = false;
            continue;
        }
        
        // Get interpolated sample value
        float value = samplerInterpolate(sample->data, sample->length, v->position);
        value *= v->volume;
        
        // Apply panning (equal power)
        float panL = cosf((v->pan + 1.0f) * 0.25f * 3.14159265f);
        float panR = sinf((v->pan + 1.0f) * 0.25f * 3.14159265f);
        outL += value * panL;
        outR += value * panR;
        
        // Advance position
        v->position += v->speed;
        
        // Handle loop or end
        if (v->loop) {
            int loopEnd = v->loopEnd > 0 ? v->loopEnd : sample->length;
            if (v->position >= loopEnd) {
                v->position = v->loopStart + (v->position - loopEnd);
            }
        } else {
            if (v->position >= sample->length) {
                v->active = false;
            }
        }
    }
    
    *left = outL * samplerCtx->volume;
    *right = outR * samplerCtx->volume;
}

// ============================================================================
// UTILITY
// ============================================================================

// Check if a sample slot is loaded
static bool samplerIsLoaded(int slotIndex) {
    _ensureSamplerCtx();
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return false;
    return samplerCtx->samples[slotIndex].loaded;
}

// Get sample name
static const char* samplerGetName(int slotIndex) {
    _ensureSamplerCtx();
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return "";
    return samplerCtx->samples[slotIndex].name;
}

// Get sample length in samples
static int samplerGetLength(int slotIndex) {
    _ensureSamplerCtx();
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return 0;
    return samplerCtx->samples[slotIndex].length;
}

// Get sample length in seconds
static float samplerGetDuration(int slotIndex) {
    _ensureSamplerCtx();
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return 0.0f;
    Sample* s = &samplerCtx->samples[slotIndex];
    if (!s->loaded || s->sampleRate == 0) return 0.0f;
    return (float)s->length / (float)s->sampleRate;
}

// Check if any voices are playing
static bool samplerIsPlaying(void) {
    _ensureSamplerCtx();
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (samplerCtx->voices[i].active) return true;
    }
    return false;
}

// Get number of active voices
static int samplerActiveVoices(void) {
    _ensureSamplerCtx();
    int count = 0;
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (samplerCtx->voices[i].active) count++;
    }
    return count;
}

#endif // PIXELSYNTH_SAMPLER_H
