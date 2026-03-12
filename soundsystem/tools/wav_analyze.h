// wav_analyze.h — Standalone WAV analysis toolkit
//
// Header-only. Provides WAV loading, spectral/temporal/pitch/character analysis,
// comparison between two buffers, and CSV/WAV export utilities.
//
// Usage:
//   #define WAV_ANALYZE_IMPLEMENTATION   // in ONE .c file
//   #include "wav_analyze.h"

#ifndef WAV_ANALYZE_H
#define WAV_ANALYZE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WA_PI
#define WA_PI 3.14159265358979323846f
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================

#define WA_SAMPLE_RATE         44100
#define WA_MAX_LOAD_SAMPLES    (WA_SAMPLE_RATE * 10)  // 10 seconds max WAV load
#define WA_SPECTRAL_BANDS      32     // Perceptual frequency bands
#define WA_SPECTRAL_FFT_LEN    4096   // DFT window size
#define WA_ENVELOPE_WINDOW_MS  1      // Envelope follower resolution
#define WA_ENVELOPE_MAX_LEN    10000  // Max envelope windows (10s @ 1ms)
#define WA_CENTROID_WINDOW_MS  10     // Spectral centroid resolution
#define WA_PARTIALS_MAX        32     // Max detected partials
#define WA_PITCH_WINDOWS_MAX   1000   // Max pitch envelope windows

// Thresholds
#define WA_SILENCE_DB          -60.0f   // Below this = silent
#define WA_SILENCE_LIN         0.001f   // 10^(-60/20)
#define WA_ATTACK_THRESHOLD    0.5f     // 50% of peak for attack time
#define WA_DECAY_DB            -20.0f   // Decay measured to -20dB below peak

// ============================================================================
// WAV I/O
// ============================================================================

// Loaded WAV data
typedef struct {
    float *data;         // Sample data (normalized -1 to 1), caller must free
    int length;          // Number of samples
    int sampleRate;      // Original sample rate
    int channels;        // Original channel count (data is always mono)
    int bitsPerSample;   // Original bit depth
} WavFile;

// Load a WAV file (any bit depth: 8/16/24/32, stereo down-mixed to mono)
// Returns true on success. Caller must free wav->data when done.
static bool waLoadWav(const char *path, WavFile *wav);

// Write mono 16-bit WAV
static bool waWriteWav(const char *path, const float *data, int numSamples, int sampleRate);

// ============================================================================
// ANALYSIS RESULTS
// ============================================================================

// Full analysis of a single audio buffer
typedef struct {
    // --- Levels ---
    float peakLevel;              // Max |sample|
    float rmsLevel;               // RMS over entire buffer
    float rmsNoteOn;              // RMS during note-on portion (if split provided)
    float rmsTail;                // RMS during release tail
    bool clipped;                 // Peak > 1.0 before clamping
    float dcOffset;               // Mean sample value
    float crestFactor;            // Peak/RMS in dB

    // --- Temporal / Envelope ---
    float attackTimeMs;           // Time to reach 50% of peak (ms), -1 if never
    float decayTimeMs;            // Time from peak to -20dB (ms), -1 if sustained
    float sustainLevel;           // Average RMS in middle 50% of note-on (0-1)
    float releaseTimeMs;          // Time from note-off to silence (ms)
    float durationMs;             // Time until signal drops below -60dB
    float envelope[WA_ENVELOPE_MAX_LEN];  // RMS envelope (1ms windows)
    int envelopeLen;              // Number of envelope windows

    // --- Spectral ---
    float bandEnergy[WA_SPECTRAL_BANDS];  // Energy per perceptual band (log-spaced 20Hz-20kHz)
    float spectralCentroid;       // Brightness in Hz (energy-weighted mean frequency)
    float spectralRolloff;        // Frequency below which 85% of energy lives (Hz)
    float spectralFlatness;       // 0 = tonal, 1 = noise (geometric/arithmetic mean ratio)
    float oddEvenRatio;           // Ratio of odd-harmonic to even-harmonic energy (>1 = hollow/square-ish)

    // --- Pitch ---
    float fundamental;            // Estimated fundamental frequency (Hz), 0 if unpitched
    float pitchConfidence;        // How clean the autocorrelation peak is (0-1)
    float pitchEnvelope[WA_PITCH_WINDOWS_MAX];  // Fundamental over time (Hz per 10ms window)
    int pitchEnvLen;              // Number of pitch windows
    float pitchDrift;             // Max deviation from mean pitch (cents)

    // --- Harmonics ---
    float partialFreqs[WA_PARTIALS_MAX];   // Detected partial frequencies (Hz)
    float partialAmps[WA_PARTIALS_MAX];    // Amplitude of each partial (0-1 relative to strongest)
    int numPartials;                        // Number of detected partials
    float inharmonicity;          // How far partials deviate from integer ratios (0=pure, 1=metallic)
    float fundamentalStrength;    // Ratio of fundamental energy to total harmonic energy

    // --- Character ---
    float noiseContent;           // Estimated noise vs tonal ratio (0=clean tone, 1=pure noise)
    float transientSharpness;     // Peak/RMS in first 5ms (higher = snappier attack)
    float zeroCrossingRate;       // Zero crossings per second
} WaAnalysis;

// Comparison between two analyses
typedef struct {
    float overall;                // Weighted overall similarity (0-1)
    float waveformSim;            // Cross-correlation of raw waveforms
    float spectralSim;            // Cosine similarity of band energy
    float envelopeSim;            // Cross-correlation of envelope contours
    float brightnessDiff;         // Centroid difference in Hz (signed: + = A brighter)
    float pitchDiff;              // Fundamental difference in cents (signed)
    float attackDiff;             // Attack time difference in ms (signed)
    float decayDiff;              // Decay time difference in ms (signed)
    float noiseDiff;              // Noise content difference (signed)
    float inharmonicityDiff;      // Inharmonicity difference (signed)
    float oddEvenDiff;            // Odd/even ratio difference (signed)
} WaComparison;

// ============================================================================
// API — ANALYSIS
// ============================================================================

// Analyze a float buffer. noteOnSamples = 0 means "entire buffer is note-on".
static WaAnalysis waAnalyze(const float *buf, int totalSamples, int noteOnSamples);

// Compare two analyses (a = your preset, b = reference)
static WaComparison waCompare(const WaAnalysis *a, const WaAnalysis *b,
                               const float *bufA, const float *bufB, int numSamples);

// ============================================================================
// API — UTILITIES
// ============================================================================

// Print analysis to stdout
static void waPrintAnalysis(const char *label, const WaAnalysis *a);

// Print comparison to stdout with parameter suggestions
static void waPrintComparison(const WaComparison *cmp, const WaAnalysis *preset,
                               const WaAnalysis *reference);

// Export waveform + envelope + spectrum as CSV
static void waExportCSV(const char *dir, const char *name,
                         const float *buf, int numSamples, const WaAnalysis *a);

// Compare-export: two buffers side by side
static void waExportCompareCSV(const char *dir, const char *name,
                                const float *bufA, const float *bufB, int numSamples,
                                const WaAnalysis *a, const WaAnalysis *b);

// ============================================================================
// HELPERS (internal)
// ============================================================================

static float wa_clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static float wa_cross_correlation(const float *a, const float *b, int n) {
    double sumAB = 0, sumA2 = 0, sumB2 = 0;
    for (int i = 0; i < n; i++) {
        sumAB += (double)a[i] * b[i];
        sumA2 += (double)a[i] * a[i];
        sumB2 += (double)b[i] * b[i];
    }
    double denom = sqrt(sumA2 * sumB2);
    if (denom < 1e-10) return 0.0f;
    return (float)(sumAB / denom);
}

// Band edge frequency for perceptual band index (log-spaced 20Hz-20kHz)
static float wa_band_freq(int band) {
    return 20.0f * powf(1000.0f, (float)band / WA_SPECTRAL_BANDS);
}

#endif // WAV_ANALYZE_H

// ============================================================================
// IMPLEMENTATION
// ============================================================================

#ifdef WAV_ANALYZE_IMPLEMENTATION

// ============================================================================
// WAV I/O
// ============================================================================

static bool waLoadWav(const char *path, WavFile *wav) {
    memset(wav, 0, sizeof(WavFile));
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char riff[4];
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
    fseek(f, 4, SEEK_CUR);  // skip file size

    char wave[4];
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }

    int channels = 1, bitsPerSample = 16, sampleRate = 44100;

    while (!feof(f)) {
        char chunkId[4];
        unsigned int chunkSize;
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            unsigned short audioFormat, numCh, bitsPS;
            unsigned int sr;
            fread(&audioFormat, 2, 1, f);
            fread(&numCh, 2, 1, f);
            fread(&sr, 4, 1, f);
            fseek(f, 4, SEEK_CUR);  // byte rate
            fseek(f, 2, SEEK_CUR);  // block align
            fread(&bitsPS, 2, 1, f);
            channels = numCh;
            bitsPerSample = bitsPS;
            sampleRate = sr;
            if (chunkSize > 16) fseek(f, chunkSize - 16, SEEK_CUR);

        } else if (memcmp(chunkId, "data", 4) == 0) {
            int bytesPerSample = bitsPerSample / 8;
            int totalSamples = (int)(chunkSize / (bytesPerSample * channels));
            if (totalSamples > WA_MAX_LOAD_SAMPLES) totalSamples = WA_MAX_LOAD_SAMPLES;

            float *data = (float *)malloc(totalSamples * sizeof(float));
            if (!data) { fclose(f); return false; }

            for (int i = 0; i < totalSamples; i++) {
                float sample = 0.0f;
                if (bitsPerSample == 16) {
                    short s; fread(&s, 2, 1, f);
                    sample = (float)s / 32768.0f;
                } else if (bitsPerSample == 8) {
                    unsigned char s; fread(&s, 1, 1, f);
                    sample = ((float)s - 128.0f) / 128.0f;
                } else if (bitsPerSample == 24) {
                    unsigned char b3[3]; fread(b3, 1, 3, f);
                    int s = b3[0] | (b3[1] << 8) | (b3[2] << 16);
                    if (s & 0x800000) s |= (int)0xFF000000;
                    sample = (float)s / 8388608.0f;
                } else if (bitsPerSample == 32) {
                    float s; fread(&s, 4, 1, f);
                    sample = s;
                }
                // Skip + average extra channels
                for (int c = 1; c < channels; c++) {
                    float extra = 0.0f;
                    if (bitsPerSample == 16) { short s; fread(&s, 2, 1, f); extra = (float)s / 32768.0f; }
                    else if (bitsPerSample == 8) { unsigned char s; fread(&s, 1, 1, f); extra = ((float)s - 128.0f) / 128.0f; }
                    else if (bitsPerSample == 24) { unsigned char b3[3]; fread(b3, 1, 3, f); int s = b3[0] | (b3[1] << 8) | (b3[2] << 16); if (s & 0x800000) s |= (int)0xFF000000; extra = (float)s / 8388608.0f; }
                    else if (bitsPerSample == 32) { float s; fread(&s, 4, 1, f); extra = s; }
                    sample = (sample + extra) * 0.5f;
                }
                data[i] = sample;
            }

            wav->data = data;
            wav->length = totalSamples;
            wav->sampleRate = sampleRate;
            wav->channels = channels;
            wav->bitsPerSample = bitsPerSample;
            fclose(f);
            return true;
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }

    fclose(f);
    return false;
}

static bool waWriteWav(const char *path, const float *data, int numSamples, int sampleRate) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    int dataSize = numSamples * 2;
    int fileSize = 36 + dataSize;
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int i32 = 16; fwrite(&i32, 4, 1, f);
    short i16 = 1; fwrite(&i16, 2, 1, f);  // PCM
    i16 = 1; fwrite(&i16, 2, 1, f);        // mono
    fwrite(&sampleRate, 4, 1, f);
    i32 = sampleRate * 2; fwrite(&i32, 4, 1, f);  // byte rate
    i16 = 2; fwrite(&i16, 2, 1, f);        // block align
    i16 = 16; fwrite(&i16, 2, 1, f);       // bits
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    for (int i = 0; i < numSamples; i++) {
        float s = data[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        short pcm = (short)(s * 32000.0f);
        fwrite(&pcm, 2, 1, f);
    }
    fclose(f);
    return true;
}

// ============================================================================
// SPECTRAL ANALYSIS
// ============================================================================

// Compute energy in a single frequency band via Goertzel-like DFT probing
static void wa_compute_band_energy(const float *buf, int n, float *bandEnergy) {
    int fftLen = n < WA_SPECTRAL_FFT_LEN ? n : WA_SPECTRAL_FFT_LEN;

    for (int band = 0; band < WA_SPECTRAL_BANDS; band++) {
        float freqLo = wa_band_freq(band);
        float freqHi = wa_band_freq(band + 1);
        float energy = 0.0f;

        // Probe 4 frequencies per band
        for (int probe = 0; probe < 4; probe++) {
            float freq = freqLo + (freqHi - freqLo) * (float)probe / 4.0f;
            float omega = 2.0f * WA_PI * freq / WA_SAMPLE_RATE;
            float realPart = 0, imagPart = 0;
            for (int i = 0; i < fftLen; i++) {
                float w = 0.5f - 0.5f * cosf(2.0f * WA_PI * (float)i / (float)fftLen);  // Hann
                float cosW = cosf(omega * (float)i);
                float sinW = sinf(omega * (float)i);
                realPart += buf[i] * w * cosW;
                imagPart += buf[i] * w * sinW;
            }
            energy += sqrtf(realPart * realPart + imagPart * imagPart);
        }
        bandEnergy[band] = energy;
    }
}

// Spectral centroid: energy-weighted average frequency
static float wa_spectral_centroid(const float *bandEnergy) {
    double freqSum = 0, ampSum = 0;
    for (int b = 0; b < WA_SPECTRAL_BANDS; b++) {
        float midFreq = (wa_band_freq(b) + wa_band_freq(b + 1)) * 0.5f;
        freqSum += midFreq * bandEnergy[b];
        ampSum += bandEnergy[b];
    }
    return ampSum > 1e-10 ? (float)(freqSum / ampSum) : 0.0f;
}

// Spectral rolloff: frequency below which 85% of energy lives
static float wa_spectral_rolloff(const float *bandEnergy) {
    double total = 0;
    for (int b = 0; b < WA_SPECTRAL_BANDS; b++) total += bandEnergy[b];
    double threshold = total * 0.85;
    double cumulative = 0;
    for (int b = 0; b < WA_SPECTRAL_BANDS; b++) {
        cumulative += bandEnergy[b];
        if (cumulative >= threshold) return wa_band_freq(b + 1);
    }
    return 20000.0f;
}

// Spectral flatness: geometric mean / arithmetic mean of band energies
// 0 = tonal (peaky spectrum), 1 = noise (flat spectrum)
static float wa_spectral_flatness(const float *bandEnergy) {
    double logSum = 0, linSum = 0;
    int count = 0;
    for (int b = 0; b < WA_SPECTRAL_BANDS; b++) {
        if (bandEnergy[b] > 1e-10f) {
            logSum += log((double)bandEnergy[b]);
            linSum += bandEnergy[b];
            count++;
        }
    }
    if (count < 2 || linSum < 1e-10) return 0.0f;
    double geoMean = exp(logSum / count);
    double ariMean = linSum / count;
    return wa_clampf((float)(geoMean / ariMean), 0.0f, 1.0f);
}

// Odd/even harmonic ratio (requires known fundamental)
static float wa_odd_even_ratio(const float *buf, int n, float fundamental) {
    if (fundamental < 20.0f || n < 256) return 1.0f;
    int fftLen = n < WA_SPECTRAL_FFT_LEN ? n : WA_SPECTRAL_FFT_LEN;
    double oddEnergy = 0, evenEnergy = 0;

    for (int h = 1; h <= 16; h++) {
        float freq = fundamental * h;
        if (freq > WA_SAMPLE_RATE * 0.45f) break;
        float omega = 2.0f * WA_PI * freq / WA_SAMPLE_RATE;
        float realPart = 0, imagPart = 0;
        for (int i = 0; i < fftLen; i++) {
            float w = 0.5f - 0.5f * cosf(2.0f * WA_PI * (float)i / (float)fftLen);
            realPart += buf[i] * w * cosf(omega * (float)i);
            imagPart += buf[i] * w * sinf(omega * (float)i);
        }
        float mag = sqrtf(realPart * realPart + imagPart * imagPart);
        if (h % 2 == 1) oddEnergy += mag; else evenEnergy += mag;
    }
    if (evenEnergy < 1e-10) return (oddEnergy > 1e-10) ? 10.0f : 1.0f;  // all odd = very hollow
    return (float)(oddEnergy / evenEnergy);
}

// ============================================================================
// PITCH DETECTION (autocorrelation)
// ============================================================================

// Detect fundamental frequency in a buffer segment via autocorrelation.
// Uses "first peak after first dip" strategy to avoid selecting sub-harmonics.
// Returns frequency in Hz, sets *confidence to peak strength (0-1).
static float wa_detect_pitch(const float *buf, int n, float *confidence) {
    if (n < 128) { *confidence = 0; return 0; }
    int analyzeLen = n < 4096 ? n : 4096;

    // Compute RMS to skip silent segments
    double rmsSum = 0;
    for (int i = 0; i < analyzeLen; i++) rmsSum += (double)buf[i] * buf[i];
    float rms = sqrtf((float)(rmsSum / analyzeLen));
    if (rms < WA_SILENCE_LIN) { *confidence = 0; return 0; }

    // Autocorrelation
    int minLag = WA_SAMPLE_RATE / 5000;  // 5kHz max
    int maxLag = WA_SAMPLE_RATE / 30;    // 30Hz min
    if (maxLag > analyzeLen / 2) maxLag = analyzeLen / 2;
    if (minLag < 2) minLag = 2;
    if (minLag >= maxLag) { *confidence = 0; return 0; }

    int corrLen = maxLag + 1;
    // Compute normalized autocorrelation for all lags
    // Use separate energy terms for proper normalization: r(t) / sqrt(r_xx(0) * r_yy(0))
    // where y is the lagged signal
    float *corr = (float *)malloc(corrLen * sizeof(float));
    if (!corr) { *confidence = 0; return 0; }

    double r0 = 0;
    int windowLen = analyzeLen - maxLag;
    for (int i = 0; i < windowLen; i++) r0 += (double)buf[i] * buf[i];

    for (int lag = 0; lag < corrLen; lag++) {
        double sum = 0, rLag = 0;
        for (int i = 0; i < windowLen; i++) {
            sum += (double)buf[i] * buf[i + lag];
            rLag += (double)buf[i + lag] * buf[i + lag];
        }
        double denom = sqrt(r0 * rLag);
        corr[lag] = (denom > 1e-10) ? (float)(sum / denom) : 0;
    }

    // Find first dip below threshold, then first peak after it
    // This avoids locking onto sub-harmonics which have higher raw correlation
    float dipThreshold = 0.5f;
    bool foundDip = false;
    int bestLag = 0;
    float bestCorr = 0;

    for (int lag = minLag; lag <= maxLag; lag++) {
        if (!foundDip) {
            if (corr[lag] < dipThreshold) foundDip = true;
            continue;
        }
        // After dip: find first clear peak (higher than neighbors and above threshold)
        if (corr[lag] > 0.3f && corr[lag] > bestCorr) {
            // Check it's a local peak (or near the start of the search)
            if (lag == minLag || corr[lag] >= corr[lag - 1]) {
                bestCorr = corr[lag];
                bestLag = lag;
                // Once we find the first strong peak, keep scanning a bit for a possibly
                // better peak nearby, but stop once correlation drops significantly
            }
        }
        // If we already found a good peak and correlation is dropping, stop
        if (bestLag > 0 && corr[lag] < bestCorr * 0.85f) break;
    }

    // Fallback: if no dip found (very low frequency or noise), use global max
    if (bestLag == 0) {
        for (int lag = minLag; lag <= maxLag; lag++) {
            if (corr[lag] > bestCorr) { bestCorr = corr[lag]; bestLag = lag; }
        }
    }

    free(corr);

    *confidence = wa_clampf(bestCorr, 0.0f, 1.0f);
    if (bestLag < 1 || bestCorr < 0.3f) { *confidence = 0; return 0; }
    return (float)WA_SAMPLE_RATE / (float)bestLag;
}

// ============================================================================
// PARTIAL DETECTION (peak-picking in DFT magnitude)
// ============================================================================

static int wa_detect_partials(const float *buf, int n, float fundamental,
                               float *freqs, float *amps, int maxPartials) {
    int fftLen = n < WA_SPECTRAL_FFT_LEN ? n : WA_SPECTRAL_FFT_LEN;
    int count = 0;
    float maxAmp = 0;

    // Probe harmonic series + nearby frequencies
    for (int h = 1; h <= maxPartials && count < maxPartials; h++) {
        float targetFreq = fundamental * h;
        if (targetFreq > WA_SAMPLE_RATE * 0.45f) break;

        // Search ±5% around expected harmonic
        float bestMag = 0, bestFreq = targetFreq;
        float searchLo = targetFreq * 0.95f;
        float searchHi = targetFreq * 1.05f;
        float step = (searchHi - searchLo) / 20.0f;

        for (float freq = searchLo; freq <= searchHi; freq += step) {
            float omega = 2.0f * WA_PI * freq / WA_SAMPLE_RATE;
            float rp = 0, ip = 0;
            for (int i = 0; i < fftLen; i++) {
                float w = 0.5f - 0.5f * cosf(2.0f * WA_PI * (float)i / (float)fftLen);
                rp += buf[i] * w * cosf(omega * (float)i);
                ip += buf[i] * w * sinf(omega * (float)i);
            }
            float mag = sqrtf(rp * rp + ip * ip);
            if (mag > bestMag) { bestMag = mag; bestFreq = freq; }
        }

        if (bestMag > 1e-6f) {
            freqs[count] = bestFreq;
            amps[count] = bestMag;
            if (bestMag > maxAmp) maxAmp = bestMag;
            count++;
        }
    }

    // Normalize amplitudes relative to strongest partial, then prune weak ones
    if (maxAmp > 1e-10f) {
        for (int i = 0; i < count; i++) amps[i] /= maxAmp;
        // Remove partials below 1% of the strongest (noise floor)
        int pruned = 0;
        for (int i = 0; i < count; i++) {
            if (amps[i] >= 0.01f) {
                freqs[pruned] = freqs[i];
                amps[pruned] = amps[i];
                pruned++;
            }
        }
        count = pruned;
    }
    return count;
}

// Compute inharmonicity: average deviation of partials from integer multiples of fundamental
static float wa_inharmonicity(const float *freqs, int numPartials, float fundamental) {
    if (numPartials < 2 || fundamental < 20.0f) return 0.0f;
    double totalCents = 0;
    int count = 0;
    for (int i = 0; i < numPartials; i++) {
        float ideal = fundamental * (i + 1);
        if (ideal < 20.0f) continue;
        float cents = fabsf(1200.0f * log2f(freqs[i] / ideal));
        totalCents += cents;
        count++;
    }
    // Normalize: 0 cents = 0, 100+ cents = 1
    return count > 0 ? wa_clampf((float)(totalCents / count) / 100.0f, 0.0f, 1.0f) : 0.0f;
}

// ============================================================================
// MAIN ANALYSIS
// ============================================================================

static WaAnalysis waAnalyze(const float *buf, int totalSamples, int noteOnSamples) {
    WaAnalysis a;
    memset(&a, 0, sizeof(a));
    if (!buf || totalSamples < 1) return a;
    if (noteOnSamples <= 0) noteOnSamples = totalSamples;

    // --- Levels ---
    double sumSq = 0, sumSqOn = 0, sumSqTail = 0, sumDC = 0;
    float peak = 0;
    for (int i = 0; i < totalSamples; i++) {
        float v = fabsf(buf[i]);
        if (v > peak) peak = v;
        sumSq += (double)buf[i] * buf[i];
        sumDC += buf[i];
        if (i < noteOnSamples) sumSqOn += (double)buf[i] * buf[i];
        else sumSqTail += (double)buf[i] * buf[i];
    }
    a.peakLevel = peak;
    a.clipped = peak > 1.0f;
    a.rmsLevel = sqrtf((float)(sumSq / totalSamples));
    a.rmsNoteOn = noteOnSamples > 0 ? sqrtf((float)(sumSqOn / noteOnSamples)) : 0;
    int tailSamples = totalSamples - noteOnSamples;
    a.rmsTail = tailSamples > 0 ? sqrtf((float)(sumSqTail / tailSamples)) : 0;
    a.dcOffset = (float)(sumDC / totalSamples);
    a.crestFactor = a.rmsLevel > 1e-5f ? 20.0f * log10f(peak / a.rmsLevel) : 0;

    // --- Attack time (time to 50% of peak) ---
    float atkThreshold = peak * WA_ATTACK_THRESHOLD;
    a.attackTimeMs = -1;
    for (int i = 0; i < totalSamples; i++) {
        if (fabsf(buf[i]) >= atkThreshold) {
            a.attackTimeMs = (float)i / WA_SAMPLE_RATE * 1000.0f;
            break;
        }
    }

    // --- Duration (last sample above -60dB) ---
    a.durationMs = 0;
    for (int i = totalSamples - 1; i >= 0; i--) {
        if (fabsf(buf[i]) > WA_SILENCE_LIN) {
            a.durationMs = (float)(i + 1) / WA_SAMPLE_RATE * 1000.0f;
            break;
        }
    }

    // --- Envelope (1ms RMS windows) ---
    int envWindow = WA_SAMPLE_RATE / 1000;  // samples per 1ms
    a.envelopeLen = totalSamples / envWindow;
    if (a.envelopeLen > WA_ENVELOPE_MAX_LEN) a.envelopeLen = WA_ENVELOPE_MAX_LEN;
    for (int w = 0; w < a.envelopeLen; w++) {
        double winSum = 0;
        for (int i = 0; i < envWindow; i++) {
            int idx = w * envWindow + i;
            if (idx < totalSamples) winSum += (double)buf[idx] * buf[idx];
        }
        a.envelope[w] = sqrtf((float)(winSum / envWindow));
    }

    // --- Decay time (from peak to -20dB below peak) ---
    float peakRMS = 0;
    int peakWindow = 0;
    for (int w = 0; w < a.envelopeLen; w++) {
        if (a.envelope[w] > peakRMS) { peakRMS = a.envelope[w]; peakWindow = w; }
    }
    float decayThreshold = peakRMS * powf(10.0f, WA_DECAY_DB / 20.0f);
    a.decayTimeMs = -1;
    for (int w = peakWindow; w < a.envelopeLen; w++) {
        if (a.envelope[w] < decayThreshold) {
            a.decayTimeMs = (float)(w - peakWindow);  // ms (1 window = 1ms)
            break;
        }
    }

    // --- Sustain level (average RMS in middle 50% of note-on) ---
    {
        int noteOnWindows = noteOnSamples / envWindow;
        int start = noteOnWindows / 4;
        int end = noteOnWindows * 3 / 4;
        if (end > start && end <= a.envelopeLen) {
            double sum = 0;
            for (int w = start; w < end; w++) sum += a.envelope[w];
            a.sustainLevel = (float)(sum / (end - start));
        }
    }

    // --- Release time (from note-off to silence) ---
    {
        int noteOffWindow = noteOnSamples / envWindow;
        if (noteOffWindow < a.envelopeLen) {
            a.releaseTimeMs = -1;
            for (int w = noteOffWindow; w < a.envelopeLen; w++) {
                if (a.envelope[w] < WA_SILENCE_LIN) {
                    a.releaseTimeMs = (float)(w - noteOffWindow);
                    break;
                }
            }
        }
    }

    // --- Zero crossing rate ---
    {
        int crossings = 0;
        int zcLen = noteOnSamples < totalSamples ? noteOnSamples : totalSamples;
        for (int i = 1; i < zcLen; i++) {
            if ((buf[i] >= 0 && buf[i-1] < 0) || (buf[i] < 0 && buf[i-1] >= 0)) crossings++;
        }
        a.zeroCrossingRate = (float)crossings / ((float)zcLen / WA_SAMPLE_RATE);
    }

    // --- Transient sharpness (peak/RMS in first 5ms) ---
    {
        int transientLen = WA_SAMPLE_RATE * 5 / 1000;
        if (transientLen > totalSamples) transientLen = totalSamples;
        float tPeak = 0;
        double tSum = 0;
        for (int i = 0; i < transientLen; i++) {
            float v = fabsf(buf[i]);
            if (v > tPeak) tPeak = v;
            tSum += (double)buf[i] * buf[i];
        }
        float tRMS = sqrtf((float)(tSum / transientLen));
        a.transientSharpness = tRMS > 1e-5f ? tPeak / tRMS : 0;
    }

    // --- Spectral analysis ---
    wa_compute_band_energy(buf, totalSamples, a.bandEnergy);
    a.spectralCentroid = wa_spectral_centroid(a.bandEnergy);
    a.spectralRolloff = wa_spectral_rolloff(a.bandEnergy);
    a.spectralFlatness = wa_spectral_flatness(a.bandEnergy);

    // --- Pitch detection ---
    // Use a stable segment (skip first 10ms of attack)
    {
        int pitchOffset = WA_SAMPLE_RATE / 100;  // 10ms
        int pitchLen = noteOnSamples - pitchOffset;
        if (pitchLen < 256) pitchLen = 256;
        if (pitchOffset + pitchLen > totalSamples) {
            pitchOffset = 0;
            pitchLen = totalSamples < 4096 ? totalSamples : 4096;
        }
        a.fundamental = wa_detect_pitch(buf + pitchOffset, pitchLen, &a.pitchConfidence);
    }

    // --- Pitch envelope (10ms windows) ---
    {
        int pitchWindow = WA_SAMPLE_RATE / 100;  // 10ms
        a.pitchEnvLen = totalSamples / pitchWindow;
        if (a.pitchEnvLen > WA_PITCH_WINDOWS_MAX) a.pitchEnvLen = WA_PITCH_WINDOWS_MAX;
        double pitchSum = 0;
        int pitchCount = 0;
        for (int w = 0; w < a.pitchEnvLen; w++) {
            float conf;
            float f = wa_detect_pitch(buf + w * pitchWindow, pitchWindow, &conf);
            a.pitchEnvelope[w] = (conf > 0.3f) ? f : 0;
            if (f > 0 && conf > 0.3f) { pitchSum += f; pitchCount++; }
        }
        // Pitch drift (max deviation in cents from mean)
        if (pitchCount > 2) {
            float meanPitch = (float)(pitchSum / pitchCount);
            float maxCents = 0;
            for (int w = 0; w < a.pitchEnvLen; w++) {
                if (a.pitchEnvelope[w] > 0) {
                    float cents = fabsf(1200.0f * log2f(a.pitchEnvelope[w] / meanPitch));
                    if (cents > maxCents) maxCents = cents;
                }
            }
            a.pitchDrift = maxCents;
        }
    }

    // --- Harmonics / partials ---
    if (a.fundamental > 20.0f) {
        a.numPartials = wa_detect_partials(buf, totalSamples, a.fundamental,
                                            a.partialFreqs, a.partialAmps, WA_PARTIALS_MAX);
        a.inharmonicity = wa_inharmonicity(a.partialFreqs, a.numPartials, a.fundamental);
        a.oddEvenRatio = a.numPartials >= 2
            ? wa_odd_even_ratio(buf, totalSamples, a.fundamental) : 1.0f;

        // Fundamental strength: energy of partial[0] vs sum of all
        if (a.numPartials >= 1) {
            float totalAmp = 0;
            for (int i = 0; i < a.numPartials; i++) totalAmp += a.partialAmps[i];
            a.fundamentalStrength = totalAmp > 0 ? a.partialAmps[0] / totalAmp : 0;
        }
    }

    // --- Noise content (spectral flatness as proxy, refined by pitch confidence) ---
    a.noiseContent = a.spectralFlatness;
    // If pitch is strong, reduce noise estimate (tonal sounds have some flat bands but are not "noisy")
    if (a.pitchConfidence > 0.5f) {
        a.noiseContent *= (1.0f - a.pitchConfidence * 0.5f);
    }

    return a;
}

// ============================================================================
// COMPARISON
// ============================================================================

static WaComparison waCompare(const WaAnalysis *a, const WaAnalysis *b,
                               const float *bufA, const float *bufB, int numSamples) {
    WaComparison cmp;
    memset(&cmp, 0, sizeof(cmp));

    // Waveform cross-correlation
    cmp.waveformSim = wa_cross_correlation(bufA, bufB, numSamples);
    if (cmp.waveformSim < 0) cmp.waveformSim = 0;

    // Spectral band similarity (cosine similarity)
    {
        double sumAB = 0, sumA2 = 0, sumB2 = 0;
        for (int i = 0; i < WA_SPECTRAL_BANDS; i++) {
            sumAB += (double)a->bandEnergy[i] * b->bandEnergy[i];
            sumA2 += (double)a->bandEnergy[i] * a->bandEnergy[i];
            sumB2 += (double)b->bandEnergy[i] * b->bandEnergy[i];
        }
        double denom = sqrt(sumA2 * sumB2);
        cmp.spectralSim = (denom > 1e-10) ? wa_clampf((float)(sumAB / denom), 0, 1) : 0;
    }

    // Envelope similarity
    {
        int envLen = a->envelopeLen < b->envelopeLen ? a->envelopeLen : b->envelopeLen;
        if (envLen > 2) {
            cmp.envelopeSim = wa_cross_correlation(a->envelope, b->envelope, envLen);
            if (cmp.envelopeSim < 0) cmp.envelopeSim = 0;
        }
    }

    // Signed differences (positive = A has more than B)
    cmp.brightnessDiff = a->spectralCentroid - b->spectralCentroid;
    cmp.attackDiff = a->attackTimeMs - b->attackTimeMs;
    cmp.decayDiff = a->decayTimeMs - b->decayTimeMs;
    cmp.noiseDiff = a->noiseContent - b->noiseContent;
    cmp.inharmonicityDiff = a->inharmonicity - b->inharmonicity;
    cmp.oddEvenDiff = a->oddEvenRatio - b->oddEvenRatio;

    // Pitch difference in cents
    if (a->fundamental > 20.0f && b->fundamental > 20.0f) {
        cmp.pitchDiff = 1200.0f * log2f(a->fundamental / b->fundamental);
    }

    // Overall: envelope 40%, spectral 35%, waveform 25%
    cmp.overall = cmp.envelopeSim * 0.40f + cmp.spectralSim * 0.35f + cmp.waveformSim * 0.25f;

    return cmp;
}

// ============================================================================
// PRINTING
// ============================================================================

static void waPrintAnalysis(const char *label, const WaAnalysis *a) {
    printf("=== %s ===\n", label);
    printf("  Peak:       %.3f (%.1f dB)%s\n", a->peakLevel,
           20.0f * log10f(fmaxf(a->peakLevel, 1e-5f)), a->clipped ? " CLIPPING" : "");
    printf("  RMS:        %.3f (%.1f dB)  [on: %.3f  tail: %.3f]\n",
           a->rmsLevel, 20.0f * log10f(fmaxf(a->rmsLevel, 1e-5f)), a->rmsNoteOn, a->rmsTail);
    printf("  Crest:      %.1f dB\n", a->crestFactor);
    if (fabsf(a->dcOffset) > 0.005f) printf("  DC offset:  %.4f\n", a->dcOffset);
    printf("  Attack:     %.1f ms\n", a->attackTimeMs);
    if (a->decayTimeMs >= 0) printf("  Decay:      %.0f ms (to -20dB)\n", a->decayTimeMs);
    if (a->sustainLevel > 0.001f) printf("  Sustain:    %.3f RMS\n", a->sustainLevel);
    printf("  Duration:   %.0f ms\n", a->durationMs);
    printf("  Pitch:      %.1f Hz (confidence %.0f%%)\n", a->fundamental, a->pitchConfidence * 100);
    if (a->pitchDrift > 0) printf("  Pitch drift:%.0f cents\n", a->pitchDrift);
    printf("  Brightness: %.0f Hz (centroid)  rolloff: %.0f Hz\n", a->spectralCentroid, a->spectralRolloff);
    printf("  Flatness:   %.2f (%s)\n", a->spectralFlatness,
           a->spectralFlatness < 0.1f ? "tonal" : a->spectralFlatness < 0.4f ? "mixed" : "noisy");
    printf("  Noise:      %.0f%%\n", a->noiseContent * 100);
    if (a->fundamental > 20.0f) {
        printf("  Odd/even:   %.2f (%s)\n", a->oddEvenRatio,
               a->oddEvenRatio > 2.0f ? "hollow/square" : a->oddEvenRatio > 1.2f ? "slightly hollow" : "balanced");
        printf("  Inharm:     %.2f (%s)\n", a->inharmonicity,
               a->inharmonicity < 0.05f ? "harmonic" : a->inharmonicity < 0.2f ? "slight" : "metallic");
        printf("  Fund str:   %.0f%%\n", a->fundamentalStrength * 100);
        printf("  Partials:   %d detected\n", a->numPartials);
    }
    printf("  Transient:  %.1f (sharpness)\n", a->transientSharpness);
    printf("  ZC rate:    %.0f/s\n", a->zeroCrossingRate);
    printf("\n");
}

static void waPrintComparison(const WaComparison *cmp, const WaAnalysis *preset,
                               const WaAnalysis *reference) {
    printf("  Overall:    %5.1f%%\n", cmp->overall * 100);
    printf("  Waveform:   %5.1f%%\n", cmp->waveformSim * 100);
    printf("  Spectrum:   %5.1f%%\n", cmp->spectralSim * 100);
    printf("  Envelope:   %5.1f%%\n", cmp->envelopeSim * 100);
    printf("\n");

    // Actionable suggestions (only print mismatches)
    printf("  Suggestions:\n");
    int suggestions = 0;

    // Brightness
    if (fabsf(cmp->brightnessDiff) > 200.0f) {
        bool tooHigh = cmp->brightnessDiff > 0;
        printf("    Brightness: preset is %s than reference (%.0f vs %.0f Hz)\n",
               tooHigh ? "brighter" : "darker", preset->spectralCentroid, reference->spectralCentroid);
        printf("      -> %s filterCutoff\n", tooHigh ? "lower" : "raise");
        if (tooHigh && preset->noiseContent > 0.2f) printf("      -> lower noiseTone or noiseMix\n");
        suggestions++;
    }

    // Attack
    if (fabsf(cmp->attackDiff) > 5.0f) {
        bool tooSlow = cmp->attackDiff > 0;
        printf("    Attack: preset is %s (%.1f vs %.1f ms)\n",
               tooSlow ? "slower" : "faster", preset->attackTimeMs, reference->attackTimeMs);
        printf("      -> %s p_attack\n", tooSlow ? "shorten" : "lengthen");
        if (!tooSlow && reference->transientSharpness > preset->transientSharpness + 0.5f)
            printf("      -> add p_clickLevel for sharper transient\n");
        suggestions++;
    }

    // Decay
    if (cmp->decayDiff != 0 && preset->decayTimeMs >= 0 && reference->decayTimeMs >= 0) {
        if (fabsf(cmp->decayDiff) > 20.0f) {
            bool tooLong = cmp->decayDiff > 0;
            printf("    Decay: preset is %s (%.0f vs %.0f ms)\n",
                   tooLong ? "longer" : "shorter", preset->decayTimeMs, reference->decayTimeMs);
            printf("      -> %s p_decay\n", tooLong ? "shorten" : "lengthen");
            suggestions++;
        }
    }

    // Noise content
    if (fabsf(cmp->noiseDiff) > 0.15f) {
        bool tooNoisy = cmp->noiseDiff > 0;
        printf("    Noise: preset is %s (%.0f%% vs %.0f%%)\n",
               tooNoisy ? "noisier" : "cleaner", preset->noiseContent * 100, reference->noiseContent * 100);
        printf("      -> %s p_noiseMix\n", tooNoisy ? "lower" : "raise");
        suggestions++;
    }

    // Odd/even (harmonic balance)
    if (fabsf(cmp->oddEvenDiff) > 0.5f && preset->fundamental > 20.0f) {
        bool tooHollow = cmp->oddEvenDiff > 0;
        printf("    Harmonics: preset is %s (odd/even %.1f vs %.1f)\n",
               tooHollow ? "more hollow" : "more even", preset->oddEvenRatio, reference->oddEvenRatio);
        if (tooHollow) {
            printf("      -> switch from square to saw, or add p_drive/p_tubeSaturation for even harmonics\n");
        } else {
            printf("      -> switch from saw to square, or increase p_pulseWidth toward 0.5\n");
        }
        suggestions++;
    }

    // Inharmonicity
    if (fabsf(cmp->inharmonicityDiff) > 0.1f && preset->fundamental > 20.0f) {
        bool tooMetallic = cmp->inharmonicityDiff > 0;
        printf("    Inharmonicity: preset is %s (%.2f vs %.2f)\n",
               tooMetallic ? "more metallic" : "more pure", preset->inharmonicity, reference->inharmonicity);
        if (tooMetallic) {
            printf("      -> lower p_fmModIndex or p_additiveInharmonicity\n");
        } else {
            printf("      -> raise p_fmModIndex, or use FM/additive with inharmonicity\n");
        }
        suggestions++;
    }

    // Pitch
    if (fabsf(cmp->pitchDiff) > 50.0f) {
        printf("    Pitch: off by %.0f cents (%.1f vs %.1f Hz)\n",
               cmp->pitchDiff, preset->fundamental, reference->fundamental);
        suggestions++;
    }

    if (suggestions == 0) {
        printf("    (none — close match!)\n");
    }
    printf("\n");
}

// ============================================================================
// CSV EXPORT
// ============================================================================

static void waExportCSV(const char *dir, const char *name,
                         const float *buf, int numSamples, const WaAnalysis *a) {
    char path[512];

    // Waveform (downsampled to ~2000 points)
    snprintf(path, sizeof(path), "%s/%s_wave.csv", dir, name);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "sample,time_ms,amplitude\n");
        int step = numSamples / 2000;
        if (step < 1) step = 1;
        for (int i = 0; i < numSamples; i += step) {
            fprintf(f, "%d,%.2f,%.6f\n", i, (float)i / WA_SAMPLE_RATE * 1000.0f, buf[i]);
        }
        fclose(f);
    }

    // Attack (first 5ms at full resolution)
    snprintf(path, sizeof(path), "%s/%s_attack.csv", dir, name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "sample,time_ms,amplitude\n");
        int atkLen = WA_SAMPLE_RATE * 5 / 1000;
        if (atkLen > numSamples) atkLen = numSamples;
        for (int i = 0; i < atkLen; i++) {
            fprintf(f, "%d,%.4f,%.6f\n", i, (float)i / WA_SAMPLE_RATE * 1000.0f, buf[i]);
        }
        fclose(f);
    }

    // Envelope
    snprintf(path, sizeof(path), "%s/%s_envelope.csv", dir, name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "window,time_ms,rms\n");
        for (int w = 0; w < a->envelopeLen; w++) {
            fprintf(f, "%d,%.1f,%.6f\n", w, (float)w, a->envelope[w]);
        }
        fclose(f);
    }

    // Spectrum (band energies)
    snprintf(path, sizeof(path), "%s/%s_spectrum.csv", dir, name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "band,freq_lo,freq_hi,energy\n");
        for (int b = 0; b < WA_SPECTRAL_BANDS; b++) {
            fprintf(f, "%d,%.1f,%.1f,%.6f\n", b, wa_band_freq(b), wa_band_freq(b+1), a->bandEnergy[b]);
        }
        fclose(f);
    }

    // Pitch envelope
    if (a->pitchEnvLen > 0) {
        snprintf(path, sizeof(path), "%s/%s_pitch.csv", dir, name);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "window,time_ms,freq_hz\n");
            for (int w = 0; w < a->pitchEnvLen; w++) {
                fprintf(f, "%d,%.1f,%.1f\n", w, (float)w * 10.0f, a->pitchEnvelope[w]);
            }
            fclose(f);
        }
    }

    // Partials
    if (a->numPartials > 0) {
        snprintf(path, sizeof(path), "%s/%s_partials.csv", dir, name);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "harmonic,freq_hz,amplitude,ideal_hz,deviation_cents\n");
            for (int i = 0; i < a->numPartials; i++) {
                float ideal = a->fundamental * (i + 1);
                float cents = (ideal > 20.0f) ? 1200.0f * log2f(a->partialFreqs[i] / ideal) : 0;
                fprintf(f, "%d,%.1f,%.4f,%.1f,%.1f\n", i+1, a->partialFreqs[i], a->partialAmps[i], ideal, cents);
            }
            fclose(f);
        }
    }
}

static void waExportCompareCSV(const char *dir, const char *name,
                                const float *bufA, const float *bufB, int numSamples,
                                const WaAnalysis *a, const WaAnalysis *b) {
    char path[512];

    // Side-by-side waveform
    snprintf(path, sizeof(path), "%s/%s_compare_wave.csv", dir, name);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "sample,time_ms,preset,reference\n");
        int step = numSamples / 2000;
        if (step < 1) step = 1;
        for (int i = 0; i < numSamples; i += step) {
            fprintf(f, "%d,%.2f,%.6f,%.6f\n", i, (float)i / WA_SAMPLE_RATE * 1000.0f, bufA[i], bufB[i]);
        }
        fclose(f);
    }

    // Side-by-side envelope
    snprintf(path, sizeof(path), "%s/%s_compare_envelope.csv", dir, name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "window,time_ms,preset_rms,reference_rms\n");
        int len = a->envelopeLen < b->envelopeLen ? a->envelopeLen : b->envelopeLen;
        for (int w = 0; w < len; w++) {
            fprintf(f, "%d,%.1f,%.6f,%.6f\n", w, (float)w, a->envelope[w], b->envelope[w]);
        }
        fclose(f);
    }

    // Side-by-side spectrum
    snprintf(path, sizeof(path), "%s/%s_compare_spectrum.csv", dir, name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "band,freq_lo,freq_hi,preset,reference\n");
        for (int i = 0; i < WA_SPECTRAL_BANDS; i++) {
            fprintf(f, "%d,%.1f,%.1f,%.6f,%.6f\n", i, wa_band_freq(i), wa_band_freq(i+1),
                    a->bandEnergy[i], b->bandEnergy[i]);
        }
        fclose(f);
    }
}

#endif // WAV_ANALYZE_IMPLEMENTATION
