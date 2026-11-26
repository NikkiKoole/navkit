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
    ACT_VISIT,          // a friend's home: knock, come in, stay, leave (visit.c)
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
    // Proposed by movers themselves (autoGames): how many it takes (minPlayers 0 = never
    // proposed) and how to set it up with them (players[0] proposed it)
    int minPlayers, maxPlayers;
    void  (*begin)(int a, const int* players, int n);
    bool  (*join)(int a, int i);   // i joins it while it runs (NULL: nobody joins); false: not now
} ActivityDef;

typedef struct {
    bool active;
    int kind;                    // ActivityKind
    int count;                   // members (movers with MoverActivity == this one)
    float age;                   // sim seconds since it started
    float timeLeft;              // ends after this many sim seconds of age (0 = until it falls apart)
    void* state;                 // the kind's own state (ActivityDef.stateSize bytes)
} Activity;

#define MAX_ACTIVITIES 256
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

// Proposed by movers themselves (game rule autoGames, where people talk: talkEnabled). A free,
// idle mover with no game cooldown and no pressing need sometimes proposes a game to free movers
// near by that it likes: the chance grows with how much it likes them and, with the social need
// on (social.h), with how lonely it is. Each one asked accepts by its own liking (and
// loneliness). Tag, hide and seek or follow the leader (two players or more). Such
// a game ends after gameMinSeconds..gameMaxSeconds; everyone leaving a game waits gameCooldown
// before the next. A free mover that sees such a game near by, with players it likes, may join
// it (gameJoinChance; the kind's join hook: tag a runner, hide and seek a hider or "found" till
// the next round, follow the leader at the back of the line), up to the kind's maxPlayers.
extern bool autoGames;
extern float gameChance;         // chance per check (each free mover ~4x/s) with a friend near by
extern float gameMinSeconds, gameMaxSeconds, gameCooldown;
extern float gameJoinChance;     // chance per check that it joins a game of friends it sees near by
extern long autoGameStarts[ACT_KIND_COUNT], autoGameDeclined, autoGameEnded, autoGameJoins;
extern long autoGamePlayersSum;  // players at the start plus every join (average per game: / starts)
extern double autoGameSecondsSum;   // how long the proposed games lasted (all ended ones)
int  ActivityStartWith(int kind, const int* players, int n, float seconds);   // -1 if it didn't start
float ActivityCooldown(int i);   // seconds before it may play again

// Queries
int  MoverActivity(int i);       // its activity, -1 if none (also -1 while waiting)
bool MoverInActivity(int i);     // in one or waiting for one (FREETIME_PLAY)
bool ActivityHas(int a, int i);  // i is a member of a
int  ActivityFirst(int kind);    // the first running activity of that kind, -1 if none
void* ActivityState(int a);      // the kind's own state

#endif // ACTIVITY_H
