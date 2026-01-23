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
    double accumTimeMs;  // Accumulated time for sections called multiple times per frame
    double history[PROFILER_HISTORY_FRAMES];
    int historyIndex;
    int historyCount;  // How many frames of data we have (up to PROFILER_HISTORY_FRAMES)
    bool active;       // Currently being measured
    bool accumulating; // If true, accumulates multiple calls per frame
    int depth;         // Hierarchy depth (0 = root, 1 = child, etc.)
    int parent;        // Index of parent section (-1 if root)
    bool collapsed;    // If true, children are hidden in UI
} ProfileSection;

#if PROFILER_ENABLED

// Global state
extern ProfileSection profilerSections[PROFILER_MAX_SECTIONS];
extern int profilerSectionCount;

void ProfileBegin(const char* name);
void ProfileEnd(const char* name);
void ProfileAccumBegin(const char* name);
void ProfileAccumEnd(const char* name);
void ProfileFrameEnd(void);
void ProfileReset(void);

double ProfileGetMin(int sectionIndex);
double ProfileGetMax(int sectionIndex);
double ProfileGetAvg(int sectionIndex);
double ProfileGetLast(int sectionIndex);
bool ProfileIsHidden(int sectionIndex);  // Returns true if any ancestor is collapsed
bool ProfileHasChildren(int sectionIndex);  // Returns true if this section has children

#define PROFILE_BEGIN(name) ProfileBegin(#name)
#define PROFILE_END(name) ProfileEnd(#name)
#define PROFILE_ACCUM_BEGIN(name) ProfileAccumBegin(#name)
#define PROFILE_ACCUM_END(name) ProfileAccumEnd(#name)
#define PROFILE_FRAME_END() ProfileFrameEnd()

#else

// No-ops when profiler is disabled at compile time
#define PROFILE_BEGIN(name) ((void)0)
#define PROFILE_END(name) ((void)0)
#define PROFILE_ACCUM_BEGIN(name) ((void)0)
#define PROFILE_ACCUM_END(name) ((void)0)
#define PROFILE_FRAME_END() ((void)0)

#endif

// ===========================================================================
// Implementation (define PROFILER_IMPLEMENTATION in one .c file before include)
// ===========================================================================

#ifdef PROFILER_IMPLEMENTATION

#if PROFILER_ENABLED

ProfileSection profilerSections[PROFILER_MAX_SECTIONS];
int profilerSectionCount = 0;
static int profilerCurrentDepth = 0;
static int profilerParentStack[PROFILER_MAX_SECTIONS];  // Stack of parent indices

static int ProfileFindOrCreate(const char* name, int depth, int parent, bool accumulating) {
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
        profilerSections[idx].accumTimeMs = 0.0;
        profilerSections[idx].historyIndex = 0;
        profilerSections[idx].historyCount = 0;
        profilerSections[idx].active = false;
        profilerSections[idx].accumulating = accumulating;
        profilerSections[idx].depth = depth;
        profilerSections[idx].parent = parent;
        profilerSections[idx].collapsed = false;
        for (int i = 0; i < PROFILER_HISTORY_FRAMES; i++) {
            profilerSections[idx].history[i] = 0.0;
        }
        return idx;
    }
    return -1;  // No space
}

void ProfileBegin(const char* name) {
    int parent = (profilerCurrentDepth > 0) ? profilerParentStack[profilerCurrentDepth - 1] : -1;
    int idx = ProfileFindOrCreate(name, profilerCurrentDepth, parent, false);
    if (idx >= 0) {
        profilerSections[idx].startTime = clock();
        profilerSections[idx].active = true;
        profilerParentStack[profilerCurrentDepth] = idx;
        profilerCurrentDepth++;
    }
}

void ProfileEnd(const char* name) {
    int idx = ProfileFindOrCreate(name, 0, -1, false);  // depth/parent don't matter for lookup
    if (idx >= 0 && profilerSections[idx].active) {
        clock_t end = clock();
        double ms = (double)(end - profilerSections[idx].startTime) / CLOCKS_PER_SEC * 1000.0;
        profilerSections[idx].lastTimeMs = ms;
        profilerSections[idx].active = false;
        if (profilerCurrentDepth > 0) profilerCurrentDepth--;
    }
}

void ProfileAccumBegin(const char* name) {
    int parent = (profilerCurrentDepth > 0) ? profilerParentStack[profilerCurrentDepth - 1] : -1;
    int idx = ProfileFindOrCreate(name, profilerCurrentDepth, parent, true);
    if (idx >= 0) {
        profilerSections[idx].startTime = clock();
        profilerSections[idx].active = true;
        // Don't push to parent stack - accum sections don't affect hierarchy during measurement
    }
}

void ProfileAccumEnd(const char* name) {
    int idx = ProfileFindOrCreate(name, 0, -1, true);  // depth/parent don't matter for lookup
    if (idx >= 0 && profilerSections[idx].active) {
        clock_t end = clock();
        double ms = (double)(end - profilerSections[idx].startTime) / CLOCKS_PER_SEC * 1000.0;
        profilerSections[idx].accumTimeMs += ms;  // Accumulate instead of replace
        profilerSections[idx].active = false;
    }
}

void ProfileFrameEnd(void) {
    // Store lastTimeMs (or accumTimeMs) into history for each section
    for (int i = 0; i < profilerSectionCount; i++) {
        ProfileSection* s = &profilerSections[i];
        // Use accumulated time for accum sections, lastTimeMs for regular sections
        double timeToStore = s->accumulating ? s->accumTimeMs : s->lastTimeMs;
        s->history[s->historyIndex] = timeToStore;
        s->historyIndex = (s->historyIndex + 1) % PROFILER_HISTORY_FRAMES;
        if (s->historyCount < PROFILER_HISTORY_FRAMES) {
            s->historyCount++;
        }
        s->lastTimeMs = 0.0;    // Reset for next frame
        s->accumTimeMs = 0.0;   // Reset accumulator for next frame
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

bool ProfileIsHidden(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return false;
    // Walk up parent chain, if any ancestor is collapsed, this section is hidden
    int parent = profilerSections[sectionIndex].parent;
    while (parent >= 0) {
        if (profilerSections[parent].collapsed) return true;
        parent = profilerSections[parent].parent;
    }
    return false;
}

bool ProfileHasChildren(int sectionIndex) {
    if (sectionIndex < 0 || sectionIndex >= profilerSectionCount) return false;
    // Check if any section has this as parent
    for (int i = 0; i < profilerSectionCount; i++) {
        if (profilerSections[i].parent == sectionIndex) return true;
    }
    return false;
}

#endif // PROFILER_ENABLED

#endif // PROFILER_IMPLEMENTATION

#endif // PROFILER_H
