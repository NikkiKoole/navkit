#include "event_log.h"
#include "../core/time.h"
#include "../simulation/weather.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static char eventLogBuffer[EVENT_LOG_MAX_ENTRIES][EVENT_LOG_MAX_LENGTH];
static int eventLogHead = 0;   // next write position
static int eventLogCount = 0;  // total entries (capped at MAX_ENTRIES)

void EventLog(const char* fmt, ...) {
    char* entry = eventLogBuffer[eventLogHead];

    // Timestamp prefix: [Season Dd HH:MM]
    int hour = (int)timeOfDay;
    int minute = (int)((timeOfDay - hour) * 60.0f);
    Season season = GetCurrentSeason();
    int dayInSeason = (GetYearDay() % daysPerSeason) + 1;
    const char* seasonName = GetSeasonName(season);
    // Abbreviate season to 3 chars
    char seasonAbbr[4] = {0};
    if (seasonName) {
        seasonAbbr[0] = seasonName[0];
        seasonAbbr[1] = seasonName[1];
        seasonAbbr[2] = seasonName[2];
    }

    int prefixLen = snprintf(entry, EVENT_LOG_MAX_LENGTH, "[%s D%d %02d:%02d] ",
                             seasonAbbr, dayInSeason, hour, minute);
    if (prefixLen < 0) prefixLen = 0;
    if (prefixLen >= EVENT_LOG_MAX_LENGTH) prefixLen = EVENT_LOG_MAX_LENGTH - 1;

    // User message
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry + prefixLen, EVENT_LOG_MAX_LENGTH - prefixLen, fmt, args);
    va_end(args);

    entry[EVENT_LOG_MAX_LENGTH - 1] = '\0';

    eventLogHead = (eventLogHead + 1) % EVENT_LOG_MAX_ENTRIES;
    if (eventLogCount < EVENT_LOG_MAX_ENTRIES) eventLogCount++;
}

void EventLogDump(const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (!f) return;

    // Write from oldest to newest
    int start;
    if (eventLogCount < EVENT_LOG_MAX_ENTRIES) {
        start = 0;
    } else {
        start = eventLogHead;  // oldest entry is at head (was overwritten next)
    }

    for (int i = 0; i < eventLogCount; i++) {
        int idx = (start + i) % EVENT_LOG_MAX_ENTRIES;
        fprintf(f, "%s\n", eventLogBuffer[idx]);
    }

    fclose(f);
}

void EventLogClear(void) {
    eventLogHead = 0;
    eventLogCount = 0;
}

int EventLogCount(void) {
    return eventLogCount;
}

const char* EventLogGet(int index) {
    if (index < 0 || index >= eventLogCount) return NULL;

    int start;
    if (eventLogCount < EVENT_LOG_MAX_ENTRIES) {
        start = 0;
    } else {
        start = eventLogHead;
    }
    int idx = (start + index) % EVENT_LOG_MAX_ENTRIES;
    return eventLogBuffer[idx];
}
