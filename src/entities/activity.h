// activity.h - joint activities: a few movers doing one thing together
// (docs/todo/social/joint-activities.md)
//
// The shared part of every group game (and later talks, walks, meals together): a pool of
// running activities, which one each mover is in, joining and leaving, one sweep that drops a
// member something else took over (a need, a job, drafting, a command), ends an activity too
// small to go on and frees its last members. Each kind is a row of hooks (ActivityDef); the
// kind's own logic and state live with it (the games in chase.c).
//
// Being in an activity is the free-time state FREETIME_PLAY: no jobs, no talk. Only a free mover
// joins (ActivityMoverFree). Transient: a load ends every activity (needs.c frees a mover left
// in FREETIME_PLAY).
#ifndef ACTIVITY_H
#define ACTIVITY_H
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ACT_TAG,
    ACT_HIDE_SEEK,
    ACT_FOLLOW_LEADER,
    ACT_PACK_HUNT,
    ACT_KIND_COUNT
} ActivityKind;

typedef struct {
    const char* name;            // "tag"
    const char* label;           // "Playing tag" (describe)
    size_t stateSize;            // the kind's own state, zeroed at start (ActivityState)
    // hooks (NULL = none)
    void  (*tick)(int a, float dt);              // the activity's logic, each sim tick
    bool  (*moverTick)(int a, int i, float dt);  // the movement loop hands member i over (true = handled)
    float (*speedScale)(int a, int i);           // walking pace x for member i (0 = no say)
    void  (*memberLeft)(int a, int i);           // i has just left (it is no longer a member): carry on
    bool  (*tooSmall)(int a);                    // can't go on (NULL: fewer than 2 members)
    void  (*end)(int a);                         // free the kind's resources (the state is freed after)
} ActivityDef;

typedef struct {
    bool active;
    int kind;                    // ActivityKind
    int count;                   // members (movers with MoverActivity == this one)
    float age;                   // sim seconds since it started
    void* state;                 // the kind's own state (ActivityDef.stateSize bytes)
} Activity;

#define MAX_ACTIVITIES 64
extern Activity activities[MAX_ACTIVITIES];
extern long playLeftCount;       // members who left an activity (stats)

const ActivityDef* ActivityDefOf(int kind);
void ClearActivities(void);      // ends every activity, frees every member
void ActivityTick(float dt);     // the sweep, then each activity's tick (after NoticeTick)
bool ActivityMoverTick(int i, float dt);   // movement loop: true = handled by its activity

// Starting and membership
int  ActivityStart(int kind);    // a new, empty activity of that kind; -1 if the pool is full
bool ActivityAdd(int a, int i);  // i joins a (leaves the one it was in); false if it isn't free
void ActivityLeave(int i);       // i leaves (its activity carries on without it, or ends)
void ActivityEnd(int a);         // ends now; its members are freed by the next sweep
bool ActivityMoverFree(int i);   // free to join: no job, no need being seen to, not drafted
// Waiting to play: joined, no activity yet (the old "player" flag: a game started next takes
// the waiting ones). The sweep frees anyone still waiting.
bool ActivityWait(int i);        // false if it isn't free
bool ActivityWaiting(int i);

// Queries
int  MoverActivity(int i);       // its activity, -1 if none (also -1 while waiting)
bool MoverInActivity(int i);     // in one or waiting for one (FREETIME_PLAY)
bool ActivityHas(int a, int i);  // i is a member of a
int  ActivityFirst(int kind);    // the first running activity of that kind, -1 if none
void* ActivityState(int a);      // the kind's own state

#endif // ACTIVITY_H
