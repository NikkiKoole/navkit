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

#define TALK_MAX_MEMBERS 4
#define MAX_TALKS 512

typedef struct {
    bool active;
    int members[TALK_MAX_MEMBERS];
    float slotX[TALK_MAX_MEMBERS], slotY[TALK_MAX_MEMBERS];   // where each member stands (pixels)
    int count;
    float centerX, centerY;   // the shared middle (o-space)
    int z;
    float timeLeft;           // sim seconds
    int speaker;              // index into members[]
    float speakTimer;         // sim seconds until the turn passes
    float age;                // sim seconds since it started
} Talk;

extern Talk talks[MAX_TALKS];
extern bool talkEnabled;
extern float talkStartRadius;   // cells
extern float talkChance;        // chance per check (each mover checks ~4x/s) that a talk starts
extern float talkMinSeconds, talkMaxSeconds, talkCooldown;
extern long talkStartedCount;   // since ClearTalks (stats)

void ClearTalks(void);
void TalkTick(float dt);                 // start / run / end conversations (after NoticeTick)
bool TalkMoverTick(int moverIdx, float dt);   // movement loop: true = handled (in a talk)
int MoverTalk(int moverIdx);             // talk index or -1
bool MoverIsSpeaking(int moverIdx);
bool TalkLookAt(int moverIdx, float* x, float* y);   // where a talking mover looks
int TalkPartner(int moverIdx);           // another member (for describe), -1 if none
void TalkStart(int a, int b);            // start a talk between two movers now (tests, scenarios)

#endif
