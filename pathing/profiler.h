#ifndef PROFILER_H
#define PROFILER_H

#include <stdbool.h>
#include <time.h>

// Compile-time toggle - set to 0 to completely remove profiler overhead
#define PROFILER_ENABLED 1

#define PROFILER_MAX_SECTIONS 32
#define PROFILER_HISTORY_FRAMES 120

typedef struct {
    const char* name;
    clock_t startTime;
    double lastTimeMs;
    double history[PROFILER_HISTORY_FRAMES];
    int historyIndex;
    int historyCount;  // How many frames of data we have (up to PROFILER_HISTORY_FRAMES)
    bool active;       // Currently being measured
} ProfileSection;

#if PROFILER_ENABLED

// Global state
extern ProfileSection profilerSections[PROFILER_MAX_SECTIONS];
extern int profilerSectionCount;

void ProfileBegin(const char* name);
void ProfileEnd(const char* name);
void ProfileFrameEnd(void);
void ProfileReset(void);

double ProfileGetMin(int sectionIndex);
double ProfileGetMax(int sectionIndex);
double ProfileGetAvg(int sectionIndex);
double ProfileGetLast(int sectionIndex);

#define PROFILE_BEGIN(name) ProfileBegin(#name)
#define PROFILE_END(name) ProfileEnd(#name)
#define PROFILE_FRAME_END() ProfileFrameEnd()

#else

// No-ops when profiler is disabled at compile time
#define PROFILE_BEGIN(name) ((void)0)
#define PROFILE_END(name) ((void)0)
#define PROFILE_FRAME_END() ((void)0)

#endif

// ===========================================================================
// Implementation (define PROFILER_IMPLEMENTATION in one .c file before include)
// ===========================================================================

#ifdef PROFILER_IMPLEMENTATION

#if PROFILER_ENABLED

ProfileSection profilerSections[PROFILER_MAX_SECTIONS];
int profilerSectionCount = 0;

static int ProfileFindOrCreate(const char* name) {
    // Find existing section
    for (int i = 0; i < profilerSectionCount; i++) {
        if (profilerSections[i].name == name) {  // Pointer comparison (string literals)
            return i;
        }
    }
    // Create new section
    if (profilerSectionCount < PROFILER_MAX_SECTIONS) {
        int idx = profilerSectionCount++;
        profilerSections[idx].name = name;
        profilerSections[idx].startTime = 0;
        profilerSections[idx].lastTimeMs = 0.0;
        profilerSections[idx].historyIndex = 0;
        profilerSections[idx].historyCount = 0;
        profilerSections[idx].active = false;
        for (int i = 0; i < PROFILER_HISTORY_FRAMES; i++) {
            profilerSections[idx].history[i] = 0.0;
        }
        return idx;
    }
    return -1;  // No space
}

void ProfileBegin(const char* name) {
    int idx = ProfileFindOrCreate(name);
    if (idx >= 0) {
        profilerSections[idx].startTime = clock();
        profilerSections[idx].active = true;
    }
}

void ProfileEnd(const char* name) {
    int idx = ProfileFindOrCreate(name);
    if (idx >= 0 && profilerSections[idx].active) {
        clock_t end = clock();
        double ms = (double)(end - profilerSections[idx].startTime) / CLOCKS_PER_SEC * 1000.0;
        profilerSections[idx].lastTimeMs = ms;
        profilerSections[idx].active = false;
    }
}

void ProfileFrameEnd(void) {
    // Store lastTimeMs into history for each section
    for (int i = 0; i < profilerSectionCount; i++) {
        ProfileSection* s = &profilerSections[i];
        s->history[s->historyIndex] = s->lastTimeMs;
        s->historyIndex = (s->historyIndex + 1) % PROFILER_HISTORY_FRAMES;
        if (s->historyCount < PROFILER_HISTORY_FRAMES) {
            s->historyCount++;
        }
        s->lastTimeMs = 0.0;  // Reset for next frame
    }
}

void ProfileReset(void) {
    profilerSectionCount = 0;
}

double ProfileGetMin(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return 0.0;
    ProfileSection* s = &profilerSections[sectionIndex];
    if (s->historyCount == 0) return 0.0;
    
    double minVal = s->history[0];
    for (int i = 1; i < s->historyCount; i++) {
        if (s->history[i] < minVal) minVal = s->history[i];
    }
    return minVal;
}

double ProfileGetMax(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return 0.0;
    ProfileSection* s = &profilerSections[sectionIndex];
    if (s->historyCount == 0) return 0.0;
    
    double maxVal = s->history[0];
    for (int i = 1; i < s->historyCount; i++) {
        if (s->history[i] > maxVal) maxVal = s->history[i];
    }
    return maxVal;
}

double ProfileGetAvg(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return 0.0;
    ProfileSection* s = &profilerSections[sectionIndex];
    if (s->historyCount == 0) return 0.0;
    
    double sum = 0.0;
    for (int i = 0; i < s->historyCount; i++) {
        sum += s->history[i];
    }
    return sum / s->historyCount;
}

double ProfileGetLast(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return 0.0;
    ProfileSection* s = &profilerSections[sectionIndex];
    if (s->historyCount == 0) return 0.0;
    
    // Get the most recently written value
    int lastIdx = (s->historyIndex - 1 + PROFILER_HISTORY_FRAMES) % PROFILER_HISTORY_FRAMES;
    return s->history[lastIdx];
}

#endif // PROFILER_ENABLED

#endif // PROFILER_IMPLEMENTATION

#endif // PROFILER_H
