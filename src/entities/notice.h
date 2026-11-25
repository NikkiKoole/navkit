// notice.h - movers notice each other (docs/todo/social/noticing.md, step 1)
//
// Every few ticks each mover looks around: other movers on the same level, within
// noticeRadius cells, in line of sight. It keeps the most salient few (attention budget),
// ranked by closeness, whether they come toward it, and whether it already noticed them
// (attention sticks). Nothing reacts to it yet except the face (looks at who it noticed
// when standing still) and the debug describe lines. Transient: rebuilt after a load.
#ifndef NOTICE_H
#define NOTICE_H

#include <stdbool.h>

#define NOTICE_MAX 4

typedef struct {
    int mover;        // index into movers[]
    double since;     // gameTime when first noticed (kept while it stays noticed)
    float salience;   // 0..~1.6, higher = more attention
} NoticeEntry;

typedef struct {
    NoticeEntry e[NOTICE_MAX];   // most salient first
    int count;
} MoverNotice;

extern MoverNotice moverNotice[];   // MAX_MOVERS
extern bool noticeEnabled;
extern float noticeRadius;          // cells
extern int noticeEveryTicks;        // each mover looks around once per this many ticks

// Can someone at cell 0 see cell 1 (same level)? Sight, not walking: walls and other solid cells
// block it; railings and windows don't (half-height / see-through), nor do furniture or a drop.
// (HasLineOfSight in mover.c is "can walk there straight": it stops at railings too.)
bool CanSeeCell(int x0, int y0, int x1, int y1, int z);

void NoticeTick(void);              // after movement (uses moverGrid)
void ClearNotices(void);
void NoticeMover(int moverIdx);     // one mover looks around now (NoticeTick staggers these)
int NoticeTop(int moverIdx);        // most salient noticed mover, -1 if none

// --- Passing politely (first social steering response) ---
// A walking mover that noticed someone coming at it (oncoming, ahead, near its line) drifts
// sideways early and slows a little: away from the side the other is on, to its right when
// dead ahead. Both do it, so they split to opposite sides instead of bumping.
extern bool politePassing;
extern float passRadius;            // cells ahead where it starts to give way
extern float passStrength;          // sideways speed at full weight, as a fraction of walking speed
extern float passSlow;              // speed lost at full weight (0..1)

// (hx, hy): unit direction the mover is walking. Out: sideways direction * weight (0..1, length
// up to ~1) and a speed scale. Returns false when there is nobody to give way to.
bool NoticePassSteer(int moverIdx, float hx, float hy, float* sideX, float* sideY, float* speedScale);

// --- Glances and greetings ---
// Walking past someone it has just noticed, a mover glances at them (its face turns, the feet
// keep going): always at someone it knows, sometimes at a stranger. Two who know each other
// (familiar, not disliked) within 4 cells greet when they meet after being apart (3 minutes out of sight): a nod
// and a smile, friends a "hi" first; it raises familiarity and like a little. Someone it
// dislikes: it looks away. Transient (not saved).
extern bool glancesEnabled, greetingsEnabled;
extern long glanceCount, greetCount;
typedef enum { GREET_NONE, GREET_NOD, GREET_HI, GREET_AWAY } GreetKind;
int NoticeGlanceAt(int moverIdx, bool* away);     // whom it is glancing at (-1 none); away: looking away
int NoticeGreeting(int moverIdx, float* phase);   // GreetKind now; phase 0..1 through it

// --- Friends and foes (relations.c) ---
// Seek: an idle wanderer that noticed someone it likes (like >= 20) sometimes walks to a spot
// near them instead of a random one. Avoid: a walker drifts sideways round someone it dislikes
// (like <= -20) ahead near its line, walking or standing; a wanderer doesn't pick a goal near them.
extern bool socialSeek, socialAvoid;
bool NoticeSocialWanderGoal(int moverIdx, int* gx, int* gy, int* gz);   // a goal near a liked one
bool NoticeGoalNearDisliked(int moverIdx, int gx, int gy, int gz);      // within 6 cells of a disliked one
bool NoticeDislikedClose(int moverIdx);   // a disliked one it noticed is within 2.5 cells: time to leave
bool NoticeAvoidSteer(int moverIdx, float hx, float hy, float* sideX, float* sideY);

#endif
