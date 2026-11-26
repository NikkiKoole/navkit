// talk.h - idle movers stop to talk (docs/todo/social/noticing.md, "Conversation groups")
//
// Two idle movers (no job, no need being handled) that noticed each other within
// talkStartRadius cells may stop: each walks to a spot on either side of their shared middle,
// they face each other and take turns speaking (the speaker's mouth moves) for
// talkMinSeconds..talkMaxSeconds, then go on, with a talkCooldown before the next talk.
// A job or a need ends it for that mover. Transient: a load ends every conversation.
#ifndef TALK_H
#define TALK_H

#include <stdbool.h>

#define TALK_MAX_MEMBERS 4      // a chat: at most 4
#define TALK_SLOTS 32           // array size: a gathering of onlookers can be bigger
#define MAX_TALKS 512

typedef enum { TALK_CHAT, TALK_WATCH, TALK_WALK } TalkKind;

typedef struct {
    bool active;
    int kind;                 // TalkKind
    int focus;                // TALK_WATCH: the mover everyone watches (stands in the middle), -1 none
    int members[TALK_SLOTS];
    float slotX[TALK_SLOTS], slotY[TALK_SLOTS];   // where each member stands (pixels)
    int spot[TALK_SLOTS];     // TALK_WATCH: ring * 64 + spot on that ring
    float stay[TALK_SLOTS];   // TALK_WATCH: seconds this onlooker still stays
    int count;
    float centerX, centerY;   // the shared middle (o-space)
    float radius;             // pixels from the middle to the members' spots
    int z;
    float timeLeft;           // sim seconds
    int speaker;              // index into members[]
    float speakTimer;         // sim seconds until the turn passes
    float age;                // sim seconds since it started
    const char* what;         // TALK_WATCH round a place: what it is ("the fire"), for describe
    float stayMin, stayMax;   // TALK_WATCH: how long onlookers stay (0 = watchMin/watchMax)
} Talk;

extern Talk talks[MAX_TALKS];
extern bool talkEnabled;
extern float talkStartRadius;   // cells
extern float talkChance;        // chance per check (each mover checks ~4x/s) that a talk starts
extern float talkMinSeconds, talkMaxSeconds, talkCooldown;
extern long talkStartedCount;   // since ClearTalks (stats)
extern long talkJoinedCount;    // movers that joined a running talk (stats)

void ClearTalks(void);
void TalkTick(float dt);                 // start / run / end conversations (after NoticeTick)
bool TalkMoverTick(int moverIdx, float dt);   // movement loop: true = handled (in a talk)
int MoverTalk(int moverIdx);             // talk index or -1
bool MoverIsSpeaking(int moverIdx);
bool TalkLookAt(int moverIdx, float* x, float* y);   // where a talking mover looks
int TalkPartner(int moverIdx);           // another member (for describe), -1 if none
int  TalkStart(int a, int b);            // start a talk between two movers now (-1: no room); bypasses who may
void TalkLeave(int moverIdx);            // take a mover out of its talk (a talk left with one ends)
void TalkSetTimeLeft(int moverIdx, float seconds);   // how long its talk goes on (a visit: the whole stay)
void TalkJoin(int t, int moverIdx);      // add a mover to talk t (room permitting)

// --- Gatherings: onlookers in rings round something worth watching (a performer, later a fire,
// someone cooking, the cat). Idle movers within watchRadius cells that can see it may come and
// watch: each takes the free spot on the innermost ring (1.5 cells out, spots ~1 cell apart;
// full -> the next ring, 1 cell further) nearest the side it came from, faces it, stays
// watchMin..watchMax s. The performer stands still and "sings" (MoverIsSpeaking).
extern float watchRadius;       // cells
extern float watchChance;       // chance per check that an idle mover in range comes to watch
extern float watchMinSeconds, watchMaxSeconds;
extern long watchJoinedCount;
int GatheringStart(int focusMover, float seconds);   // -1 if none free
// A gathering round a place (a campfire): people stand in the rings and face it; what = how
// describe names it ("the fire"); stay 0 = watchMin/watchMax
int GatheringStartAt(float x, float y, int z, float seconds, const char* what, float stayMin, float stayMax);
bool MoverIsWatching(int moverIdx);                  // an onlooker (not the performer)
bool MoverIsPerforming(int moverIdx);

// --- Walking together (TALK_WALK): an idle mover outdoors that noticed an idle one it likes
// (like >= walkMinLike) within 4 cells may suggest a walk: it leads (members[0]) along its own path to a
// spot 6-14 cells away at a stroll (walkPace), slowing when the other falls behind; the other
// (members[1]) keeps a spot at the leader's right, or behind it where there's no room. They
// chat on the way. Ends on arrival, after walkMaxSeconds, a job/need, or if they get separated.
extern float walkChance, walkMinLike, walkMaxSeconds, walkPace;
extern long walkStartedCount, walkJoinedCount;   // walks started; movers that joined a walking group
bool TalkWalkStart(int leader, int follower);   // false if the leader found nowhere to go
float TalkWalkSpeedScale(int moverIdx);         // the leader's pace (1 if not leading a walk)
bool MoverIsWalkingWith(int moverIdx);
bool TalkWalkSlot(int t, int k, float* x, float* y);
bool TalkWalkJoin(int t, int moverIdx);   // join walking group t (up to 6), false if no room   // where follower k of walk t should be (stats, tests)

// --- Table talk: movers eating (or lingering after) seated (sit-then-eat, action_queue.c) next to the same table
// chat with each other: the speaker takes turns, they look at the table and get to know each
// other (half a conversation's rate). They stay busy with their meal; no talk record.
extern bool tableTalkEnabled;
extern long tableTalkTicks;          // diner-ticks with someone to talk to at the table (stats)
int MoverDiningAt(int moverIdx);     // the table it eats at (furniture index), -1 if none
void TableTalkTick(float dt);        // (TalkTick calls it)

// --- Gatherings the world starts by itself (a game rule, autoGatherings): round a fire
// workshop while it burns (campfire, fire pit, ground fire, hearth), round heat furniture
// (burning barrel, heating grate), and round someone cooking (at work at a fire or stove).
// Started when the source becomes active, ended when it stops; none where a gathering already is.
extern bool autoGatherings;
extern long autoGatheringsStarted;
void AutoGatherTick(float dt);   // (TalkTick calls it)

// --- Mingling: someone in a talk or round a place (a campfire) now and then moves over to a
// group within 10 cells it likes clearly better (the one it likes most there, +15 over here; a
// place counts +10 of its own), after 6 s at least; when a talk ends its members look for a
// group with a friend in it before wandering off. It walks to the group and joins when in reach
// (up to 15 s). Not from walks, not the performer. Setting mingleEnabled.
extern bool mingleEnabled;
extern float mingleChance;          // per check, once a clearly better group is in range
extern long mingleCount;            // moves to another group (stats)
bool MoverIsMingling(int moverIdx); // on its way to another group

// --- Choosing a spot (a seat, a place in a gathering's ring) by who's next to it ---
// A bonus in cells (positive: nicer, negative: rather not) from the people within 1.6 cells of
// the spot (sitting/reserved on seats there, or standing there): a friend (like >= 20) up to +4,
// someone it knows +0.3, a stranger -0.5 (you'd rather not sit right next to one), someone it
// dislikes -4 and worse. Spot finders take the best of distance minus this. Setting spotByFriends.
extern bool spotByFriends;
float SpotPreference(int moverIdx, float x, float y, int z);
float SeatPreference(int moverIdx, int furnitureIdx);   // (action_queue.c's seat finder)

// Passers-by give a conversation room: a walker with a talk's middle ahead, near its line,
// drifts sideways around the circle (like polite passing). Out: sideways direction * weight.
extern bool talkGiveRoom;
bool TalkGiveRoomSteer(int moverIdx, float hx, float hy, float* sideX, float* sideY);

#endif
