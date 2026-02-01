// Sample Embed Tool - Scans a directory for .wav files and generates a C header
// with embedded sample data for the sampler engine.
//
// Usage: sample_embed <samples_directory> > sample_data.h
//
// Build: clang -o sample_embed sample_embed.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_SAMPLES 128
#define MAX_PATH 512
#define MAX_SAMPLE_LENGTH 262144  // ~5.4 seconds at 48kHz

typedef struct {
    char path[MAX_PATH];
    char name[128];
    char category[128];
    char varName[128];
    char shortName[16];
    float* data;
    int length;
    int sampleRate;
} SampleEntry;

static SampleEntry entries[MAX_SAMPLES];
static int entryCount = 0;

// Convert string to valid C identifier
static void toIdentifier(const char* src, char* dst) {
    int j = 0;
    for (int i = 0; src[i] && j < 126; i++) {
        char c = src[i];
        if (isalnum(c)) {
            dst[j++] = tolower(c);
        } else if (c == ' ' || c == '-' || c == '_' || c == '/') {
            if (j > 0 && dst[j-1] != '_') {
                dst[j++] = '_';
            }
        }
    }
    // Remove trailing underscore
    while (j > 0 && dst[j-1] == '_') j--;
    dst[j] = '\0';
}

// Generate a short name for UI display (max 8 chars)
static void toShortName(const char* category, const char* name, char* dst) {
    // Try to create something like "sKick" or "sCowB"
    dst[0] = 's';  // 's' prefix for sample
    
    // Use first letter of category if it's short
    int pos = 1;
    
    // Copy name, abbreviated
    for (int i = 0; name[i] && pos < 7; i++) {
        char c = name[i];
        if (isalnum(c)) {
            if (pos == 1) {
                dst[pos++] = toupper(c);
            } else {
                dst[pos++] = c;
            }
        }
    }
    dst[pos] = '\0';
    (void)category;  // Could use category prefix later
}

// Read a WAV file and extract samples as floats
static int readWav(const char* path, float** outData, int* outSampleRate) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    
    // Read RIFF header
    char riff[4];
    fread(riff, 1, 4, f);
    if (memcmp(riff, "RIFF", 4) != 0) {
        fclose(f);
        return 0;
    }
    
    fseek(f, 4, SEEK_CUR); // Skip file size
    
    char wave[4];
    fread(wave, 1, 4, f);
    if (memcmp(wave, "WAVE", 4) != 0) {
        fclose(f);
        return 0;
    }
    
    // Find fmt chunk
    int channels = 1;
    int bitsPerSample = 16;
    int sampleRate = 44100;
    
    while (!feof(f)) {
        char chunkId[4];
        unsigned int chunkSize;
        
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;
        
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            unsigned short audioFormat, numChannels;
            unsigned int sr;
            unsigned short blockAlign, bitsPS;
            
            fread(&audioFormat, 2, 1, f);
            fread(&numChannels, 2, 1, f);
            fread(&sr, 4, 1, f);
            fseek(f, 4, SEEK_CUR); // Skip byte rate
            fread(&blockAlign, 2, 1, f);
            fread(&bitsPS, 2, 1, f);
            
            channels = numChannels;
            bitsPerSample = bitsPS;
            sampleRate = sr;
            *outSampleRate = sampleRate;
            
            // Skip rest of fmt chunk if any
            if (chunkSize > 16) {
                fseek(f, chunkSize - 16, SEEK_CUR);
            }
        } else if (memcmp(chunkId, "data", 4) == 0) {
            // Read sample data
            int bytesPerSample = bitsPerSample / 8;
            int totalSamples = chunkSize / (bytesPerSample * channels);
            if (totalSamples > MAX_SAMPLE_LENGTH) totalSamples = MAX_SAMPLE_LENGTH;
            
            float* data = (float*)malloc(totalSamples * sizeof(float));
            if (!data) {
                fclose(f);
                return 0;
            }
            
            for (int i = 0; i < totalSamples; i++) {
                float sample = 0.0f;
                float rightSample = 0.0f;
                
                if (bitsPerSample == 16) {
                    short s;
                    fread(&s, 2, 1, f);
                    sample = (float)s / 32768.0f;
                    for (int c = 1; c < channels; c++) {
                        fread(&s, 2, 1, f);
                        rightSample = (float)s / 32768.0f;
                    }
                } else if (bitsPerSample == 8) {
                    unsigned char s;
                    fread(&s, 1, 1, f);
                    sample = ((float)s - 128.0f) / 128.0f;
                    for (int c = 1; c < channels; c++) {
                        fread(&s, 1, 1, f);
                        rightSample = ((float)s - 128.0f) / 128.0f;
                    }
                } else if (bitsPerSample == 32) {
                    float s;
                    fread(&s, 4, 1, f);
                    sample = s;
                    for (int c = 1; c < channels; c++) {
                        fread(&s, 4, 1, f);
                        rightSample = s;
                    }
                } else if (bitsPerSample == 24) {
                    unsigned char bytes[3];
                    fread(bytes, 1, 3, f);
                    int s = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
                    if (s & 0x800000) s |= 0xFF000000; // Sign extend
                    sample = (float)s / 8388608.0f;
                    for (int c = 1; c < channels; c++) {
                        fread(bytes, 1, 3, f);
                        s = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
                        if (s & 0x800000) s |= 0xFF000000;
                        rightSample = (float)s / 8388608.0f;
                    }
                }
                
                // Average stereo to mono
                if (channels > 1) {
                    sample = (sample + rightSample) * 0.5f;
                }
                
                data[i] = sample;
            }
            
            *outData = data;
            fclose(f);
            return totalSamples;
        } else {
            // Skip unknown chunk
            fseek(f, chunkSize, SEEK_CUR);
        }
    }
    
    fclose(f);
    return 0;
}

// Recursively scan directory for .wav files
static void scanDirectory(const char* basePath, const char* relPath) {
    char fullPath[MAX_PATH];
    if (relPath[0]) {
        snprintf(fullPath, MAX_PATH, "%s/%s", basePath, relPath);
    } else {
        snprintf(fullPath, MAX_PATH, "%s", basePath);
    }
    
    DIR* dir = opendir(fullPath);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && entryCount < MAX_SAMPLES) {
        if (entry->d_name[0] == '.') continue;
        
        char entryRelPath[MAX_PATH];
        if (relPath[0]) {
            snprintf(entryRelPath, MAX_PATH, "%s/%s", relPath, entry->d_name);
        } else {
            snprintf(entryRelPath, MAX_PATH, "%s", entry->d_name);
        }
        
        char entryFullPath[MAX_PATH];
        snprintf(entryFullPath, MAX_PATH, "%s/%s", basePath, entryRelPath);
        
        struct stat st;
        if (stat(entryFullPath, &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            scanDirectory(basePath, entryRelPath);
        } else if (S_ISREG(st.st_mode)) {
            // Check if .wav file
            int len = strlen(entry->d_name);
            if (len > 4 && strcasecmp(entry->d_name + len - 4, ".wav") == 0) {
                SampleEntry* e = &entries[entryCount];
                
                strncpy(e->path, entryFullPath, MAX_PATH - 1);
                
                // Extract name (filename without extension)
                strncpy(e->name, entry->d_name, 127);
                e->name[strlen(e->name) - 4] = '\0'; // Remove .wav
                
                // Extract category from path
                if (relPath[0]) {
                    char* lastSlash = strrchr(entryRelPath, '/');
                    if (lastSlash) {
                        int catLen = lastSlash - entryRelPath;
                        if (catLen > 127) catLen = 127;
                        strncpy(e->category, entryRelPath, catLen);
                        e->category[catLen] = '\0';
                    } else {
                        strcpy(e->category, "samples");
                    }
                } else {
                    strcpy(e->category, "samples");
                }
                
                // Generate variable name
                char combined[256];
                snprintf(combined, 256, "%s_%s", e->category, e->name);
                toIdentifier(combined, e->varName);
                
                // Generate short name for UI
                toShortName(e->category, e->name, e->shortName);
                
                // Read the wav data
                e->data = NULL;
                e->length = readWav(e->path, &e->data, &e->sampleRate);
                
                if (e->length > 0 && e->data) {
                    entryCount++;
                    fprintf(stderr, "Found: %s (%d samples @ %dHz)\n", 
                            entryRelPath, e->length, e->sampleRate);
                } else {
                    fprintf(stderr, "Warning: Could not read %s\n", entryRelPath);
                    if (e->data) free(e->data);
                }
            }
        }
    }
    
    closedir(dir);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <samples_directory>\n", argv[0]);
        fprintf(stderr, "\nScans directory recursively for .wav files and generates\n");
        fprintf(stderr, "a C header with embedded sample data.\n");
        fprintf(stderr, "\nExample: %s soundsystem/oneshots > soundsystem/engines/sample_data.h\n", argv[0]);
        return 1;
    }
    
    const char* samplesDir = argv[1];
    
    fprintf(stderr, "Scanning %s for .wav files...\n", samplesDir);
    scanDirectory(samplesDir, "");
    fprintf(stderr, "Found %d samples\n", entryCount);
    
    if (entryCount == 0) {
        fprintf(stderr, "No samples found!\n");
        return 1;
    }
    
    // Calculate total size
    size_t totalBytes = 0;
    for (int i = 0; i < entryCount; i++) {
        totalBytes += entries[i].length * sizeof(float);
    }
    fprintf(stderr, "Total embedded size: %.2f KB\n", totalBytes / 1024.0f);
    
    // Generate header
    printf("// Auto-generated by sample_embed - do not edit manually\n");
    printf("// Source: %s\n", samplesDir);
    printf("// Samples: %d\n", entryCount);
    printf("// Total size: %.2f KB\n", totalBytes / 1024.0f);
    printf("\n");
    printf("#ifndef SAMPLE_DATA_H\n");
    printf("#define SAMPLE_DATA_H\n");
    printf("\n");
    printf("#include <stdbool.h>\n");
    printf("\n");
    
    // Embedded sample struct
    printf("// Embedded sample metadata\n");
    printf("typedef struct {\n");
    printf("    const char* name;       // Display name\n");
    printf("    const char* shortName;  // Short name for UI (max 8 chars)\n");
    printf("    const char* category;   // Category/folder name\n");
    printf("    const float* data;      // Sample data\n");
    printf("    int length;             // Number of samples\n");
    printf("    int sampleRate;         // Original sample rate\n");
    printf("} EmbeddedSample;\n");
    printf("\n");
    
    // Output each sample's data
    for (int i = 0; i < entryCount; i++) {
        SampleEntry* e = &entries[i];
        printf("static const float sample_%s[%d] = {\n    ", e->varName, e->length);
        for (int j = 0; j < e->length; j++) {
            printf("%.6ff", e->data[j]);
            if (j < e->length - 1) {
                printf(",");
                if ((j + 1) % 8 == 0) {
                    printf("\n    ");
                } else {
                    printf(" ");
                }
            }
        }
        printf("\n};\n\n");
    }
    
    // Output the table
    printf("static const EmbeddedSample embeddedSamples[%d] = {\n", entryCount);
    for (int i = 0; i < entryCount; i++) {
        SampleEntry* e = &entries[i];
        printf("    {\"%s\", \"%s\", \"%s\", sample_%s, %d, %d}", 
               e->name, e->shortName, e->category, e->varName, e->length, e->sampleRate);
        if (i < entryCount - 1) printf(",");
        printf("\n");
    }
    printf("};\n");
    printf("\n");
    printf("#define EMBEDDED_SAMPLE_COUNT %d\n", entryCount);
    printf("\n");
    
    // Helper function to load embedded samples into the sampler
    printf("// Load all embedded samples into the sampler engine\n");
    printf("// Returns the number of samples loaded\n");
    printf("static int loadEmbeddedSamples(void) {\n");
    printf("    _ensureSamplerCtx();\n");
    printf("    int loaded = 0;\n");
    printf("    for (int i = 0; i < EMBEDDED_SAMPLE_COUNT && i < SAMPLER_MAX_SAMPLES; i++) {\n");
    printf("        const EmbeddedSample* e = &embeddedSamples[i];\n");
    printf("        Sample* s = &samplerCtx->samples[i];\n");
    printf("        \n");
    printf("        // Point to embedded data (no copy needed, it's const)\n");
    printf("        s->data = (float*)e->data;  // Cast away const - data is read-only\n");
    printf("        s->length = e->length;\n");
    printf("        s->sampleRate = e->sampleRate;\n");
    printf("        s->loaded = true;\n");
    printf("        s->embedded = true;  // Don't free this data\n");
    printf("        strncpy(s->name, e->name, sizeof(s->name) - 1);\n");
    printf("        s->name[sizeof(s->name) - 1] = '\\0';\n");
    printf("        loaded++;\n");
    printf("    }\n");
    printf("    return loaded;\n");
    printf("}\n");
    printf("\n");
    
    // Helper to get embedded sample info
    printf("// Get embedded sample info by index\n");
    printf("static const EmbeddedSample* getEmbeddedSampleInfo(int index) {\n");
    printf("    if (index < 0 || index >= EMBEDDED_SAMPLE_COUNT) return NULL;\n");
    printf("    return &embeddedSamples[index];\n");
    printf("}\n");
    printf("\n");
    
    printf("#endif // SAMPLE_DATA_H\n");
    
    // Free allocated data
    for (int i = 0; i < entryCount; i++) {
        if (entries[i].data) free(entries[i].data);
    }
    
    fprintf(stderr, "Done! Header written to stdout.\n");
    return 0;
}
