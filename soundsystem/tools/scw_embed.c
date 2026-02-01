// SCW Embed Tool - Scans a directory for .wav files and generates a C header
// with embedded waveform data.
//
// Usage: scw_embed <cycles_directory> > scw_data.h
//
// Build: clang -o scw_embed scw_embed.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_SCW 256
#define MAX_PATH 512
#define MAX_SAMPLES 2048

typedef struct {
    char path[MAX_PATH];
    char name[128];
    char category[128];
    char varName[128];
    float data[MAX_SAMPLES];
    int size;
} SCWEntry;

static SCWEntry entries[MAX_SCW];
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

// Read a WAV file and extract samples as floats
static int readWav(const char* path, float* data, int maxSamples) {
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
    
    while (!feof(f)) {
        char chunkId[4];
        unsigned int chunkSize;
        
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;
        
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            unsigned short audioFormat, numChannels;
            unsigned int sampleRate;
            unsigned short blockAlign, bitsPS;
            
            fread(&audioFormat, 2, 1, f);
            fread(&numChannels, 2, 1, f);
            fread(&sampleRate, 4, 1, f);
            fseek(f, 4, SEEK_CUR); // Skip byte rate
            fread(&blockAlign, 2, 1, f);
            fread(&bitsPS, 2, 1, f);
            
            channels = numChannels;
            bitsPerSample = bitsPS;
            
            // Skip rest of fmt chunk if any
            if (chunkSize > 16) {
                fseek(f, chunkSize - 16, SEEK_CUR);
            }
        } else if (memcmp(chunkId, "data", 4) == 0) {
            // Read sample data
            int bytesPerSample = bitsPerSample / 8;
            int totalSamples = chunkSize / (bytesPerSample * channels);
            if (totalSamples > maxSamples) totalSamples = maxSamples;
            
            for (int i = 0; i < totalSamples; i++) {
                float sample = 0.0f;
                
                if (bitsPerSample == 16) {
                    short s;
                    fread(&s, 2, 1, f);
                    sample = (float)s / 32768.0f;
                    // Skip extra channels
                    for (int c = 1; c < channels; c++) {
                        fseek(f, 2, SEEK_CUR);
                    }
                } else if (bitsPerSample == 8) {
                    unsigned char s;
                    fread(&s, 1, 1, f);
                    sample = ((float)s - 128.0f) / 128.0f;
                    for (int c = 1; c < channels; c++) {
                        fseek(f, 1, SEEK_CUR);
                    }
                } else if (bitsPerSample == 32) {
                    float s;
                    fread(&s, 4, 1, f);
                    sample = s;
                    for (int c = 1; c < channels; c++) {
                        fseek(f, 4, SEEK_CUR);
                    }
                } else if (bitsPerSample == 24) {
                    unsigned char bytes[3];
                    fread(bytes, 1, 3, f);
                    int s = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
                    if (s & 0x800000) s |= 0xFF000000; // Sign extend
                    sample = (float)s / 8388608.0f;
                    for (int c = 1; c < channels; c++) {
                        fseek(f, 3, SEEK_CUR);
                    }
                }
                
                data[i] = sample;
            }
            
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
    while ((entry = readdir(dir)) != NULL && entryCount < MAX_SCW) {
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
                SCWEntry* e = &entries[entryCount];
                
                strncpy(e->path, entryFullPath, MAX_PATH - 1);
                
                // Extract name (filename without extension)
                strncpy(e->name, entry->d_name, 127);
                e->name[strlen(e->name) - 4] = '\0'; // Remove .wav
                
                // Extract category from path
                if (relPath[0]) {
                    // Find the directory part
                    char* lastSlash = strrchr(entryRelPath, '/');
                    if (lastSlash) {
                        int catLen = lastSlash - entryRelPath;
                        if (catLen > 127) catLen = 127;
                        strncpy(e->category, entryRelPath, catLen);
                        e->category[catLen] = '\0';
                    } else {
                        strcpy(e->category, "Default");
                    }
                } else {
                    strcpy(e->category, "Default");
                }
                
                // Generate variable name
                char combined[256];
                snprintf(combined, 256, "%s_%s", e->category, e->name);
                toIdentifier(combined, e->varName);
                
                // Read the wav data
                e->size = readWav(e->path, e->data, MAX_SAMPLES);
                
                if (e->size > 0) {
                    entryCount++;
                    fprintf(stderr, "Found: %s (%d samples)\n", entryRelPath, e->size);
                } else {
                    fprintf(stderr, "Warning: Could not read %s\n", entryRelPath);
                }
            }
        }
    }
    
    closedir(dir);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cycles_directory>\n", argv[0]);
        return 1;
    }
    
    const char* cyclesDir = argv[1];
    
    fprintf(stderr, "Scanning %s for .wav files...\n", cyclesDir);
    scanDirectory(cyclesDir, "");
    fprintf(stderr, "Found %d waveforms\n", entryCount);
    
    if (entryCount == 0) {
        fprintf(stderr, "No waveforms found!\n");
        return 1;
    }
    
    // Generate header
    printf("// Auto-generated by scw_embed - do not edit manually\n");
    printf("// Source: %s\n", cyclesDir);
    printf("// Waveforms: %d\n", entryCount);
    printf("\n");
    printf("#ifndef SCW_DATA_H\n");
    printf("#define SCW_DATA_H\n");
    printf("\n");
    printf("#include <stdbool.h>\n");
    printf("\n");
    
    // Embedded SCW struct
    printf("typedef struct {\n");
    printf("    const char* name;\n");
    printf("    const char* category;\n");
    printf("    const float* data;\n");
    printf("    int size;\n");
    printf("} EmbeddedSCW;\n");
    printf("\n");
    
    // Output each waveform's data
    for (int i = 0; i < entryCount; i++) {
        SCWEntry* e = &entries[i];
        printf("static const float scw_%s[%d] = {\n    ", e->varName, e->size);
        for (int j = 0; j < e->size; j++) {
            printf("%.6ff", e->data[j]);
            if (j < e->size - 1) {
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
    printf("static const EmbeddedSCW embeddedSCWs[%d] = {\n", entryCount);
    for (int i = 0; i < entryCount; i++) {
        SCWEntry* e = &entries[i];
        printf("    {\"%s\", \"%s\", scw_%s, %d}", e->name, e->category, e->varName, e->size);
        if (i < entryCount - 1) printf(",");
        printf("\n");
    }
    printf("};\n");
    printf("\n");
    printf("#define EMBEDDED_SCW_COUNT %d\n", entryCount);
    printf("\n");
    
    // Helper function to load embedded SCWs into the synth's SCW table
    printf("// Load all embedded SCWs into the synth's wavetable slots\n");
    printf("// Call this instead of loadSCW() calls\n");
    printf("static int loadEmbeddedSCWs(void) {\n");
    printf("    _ensureSynthCtx();\n");
    printf("    int loaded = 0;\n");
    printf("    for (int i = 0; i < EMBEDDED_SCW_COUNT && scwCount < SCW_MAX_SLOTS; i++) {\n");
    printf("        const EmbeddedSCW* e = &embeddedSCWs[i];\n");
    printf("        SCWTable* table = &scwTables[scwCount];\n");
    printf("        \n");
    printf("        // Copy data\n");
    printf("        int size = e->size;\n");
    printf("        if (size > SCW_MAX_SIZE) size = SCW_MAX_SIZE;\n");
    printf("        for (int j = 0; j < size; j++) {\n");
    printf("            table->data[j] = e->data[j];\n");
    printf("        }\n");
    printf("        table->size = size;\n");
    printf("        table->loaded = true;\n");
    printf("        table->name = e->name;\n");
    printf("        scwCount++;\n");
    printf("        loaded++;\n");
    printf("    }\n");
    printf("    return loaded;\n");
    printf("}\n");
    printf("\n");
    printf("#endif // SCW_DATA_H\n");
    
    fprintf(stderr, "Done! Header written to stdout.\n");
    return 0;
}
