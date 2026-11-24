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

#endif
