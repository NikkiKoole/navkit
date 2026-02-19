#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stdbool.h>

#define EVENT_LOG_MAX_ENTRIES 4096
#define EVENT_LOG_MAX_LENGTH  200

void EventLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void EventLogDump(const char* filepath);
void EventLogClear(void);
int  EventLogCount(void);

// Get entry by index (0 = oldest in buffer). Returns NULL if out of range.
const char* EventLogGet(int index);

#endif
