// PixelSynth - Shared file I/O helpers for .song and .daw formats
// Write/read key=value pairs, strip whitespace, parse primitives
//
// Used by song_file.h and daw_file.h — include after <stdio.h>, <string.h>, <ctype.h>

#ifndef PIXELSYNTH_FILE_HELPERS_H
#define PIXELSYNTH_FILE_HELPERS_H

// ============================================================================
// WRITE HELPERS
// ============================================================================

static void fileWriteFloat(FILE *f, const char *key, float val) {
    fprintf(f, "%s = %.6g\n", key, (double)val);
}

static void fileWriteInt(FILE *f, const char *key, int val) {
    fprintf(f, "%s = %d\n", key, val);
}

static void fileWriteBool(FILE *f, const char *key, bool val) {
    fprintf(f, "%s = %s\n", key, val ? "true" : "false");
}

static void fileWriteStr(FILE *f, const char *key, const char *val) {
    if (val[0]) fprintf(f, "%s = \"%s\"\n", key, val);
}

static void fileWriteEnum(FILE *f, const char *key, int val, const char **names, int count) {
    if (val >= 0 && val < count) fprintf(f, "%s = %s\n", key, names[val]);
    else fprintf(f, "%s = %d\n", key, val);
}

// ============================================================================
// READ HELPERS
// ============================================================================

static float fileParseFloat(const char *val) { return (float)atof(val); }
static int fileParseInt(const char *val) { return atoi(val); }

static bool fileParseBool(const char *val) {
    return (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0);
}

// Strip leading/trailing whitespace in-place, return new start
static char* fileStrip(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
    return s;
}

// Strip surrounding quotes if present
static void fileStripQuotes(char *s) {
    int len = (int)strlen(s);
    if (len >= 2 && s[0] == '"' && s[len-1] == '"') {
        memmove(s, s+1, len-2);
        s[len-2] = '\0';
    }
}

// Lookup name in a string table (case-insensitive), return index or -1
static int fileLookupName(const char *name, const char **table, int count) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(name, table[i]) == 0) return i;
    }
    return -1;
}

#endif // PIXELSYNTH_FILE_HELPERS_H
