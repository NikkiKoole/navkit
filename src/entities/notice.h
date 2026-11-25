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

#endif
