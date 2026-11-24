// notice.c - movers notice each other (see notice.h)
#include "notice.h"
#include "mover.h"
#include "../core/time.h"
#include <math.h>
#include <string.h>

MoverNotice moverNotice[MAX_MOVERS];
bool noticeEnabled = true;
float noticeRadius = 6.0f;
int noticeEveryTicks = 15;

static unsigned int noticeTickCounter = 0;

// Salience bonuses on top of closeness (1 at the same spot, 0 at the radius edge)
#define NOTICE_STICKY_BONUS    0.35f   // already noticed: attention doesn't flick between equals
#define NOTICE_APPROACH_BONUS  0.25f   // walking toward me

void ClearNotices(void) {
    memset(moverNotice, 0, sizeof(moverNotice));
    noticeTickCounter = 0;
}

int NoticeTop(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return -1;
    const MoverNotice* n = &moverNotice[moverIdx];
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j >= 0 && j < moverCount && movers[j].active) return j;
    }
    return -1;
}

// Is mover j walking toward point (x, y)? Its heading is the direction to its next waypoint.
static bool HeadingToward(int j, float x, float y) {
    const Mover* o = &movers[j];
    if (o->pathLength <= 0 || o->pathIndex < 0) return false;
    Point wp = moverPaths[j][o->pathIndex];
    float hx = wp.x * CELL_SIZE + CELL_SIZE * 0.5f - o->x;
    float hy = wp.y * CELL_SIZE + CELL_SIZE * 0.5f - o->y;
    float tx = x - o->x, ty = y - o->y;
    float hl = sqrtf(hx * hx + hy * hy), tl = sqrtf(tx * tx + ty * ty);
    if (hl < 0.5f || tl < 0.5f) return false;
    return (hx * tx + hy * ty) / (hl * tl) > 0.7f;
}

static const NoticeEntry* FindEntry(const MoverNotice* n, int j) {
    for (int k = 0; k < n->count; k++) if (n->e[k].mover == j) return &n->e[k];
    return NULL;
}

void NoticeMover(int i) {
    MoverNotice* n = &moverNotice[i];
    const Mover* m = &movers[i];
    if (!noticeEnabled || !m->active || !moverGrid.cellCounts) { n->count = 0; return; }

    MoverNotice fresh = {0};
    float radius = noticeRadius * CELL_SIZE;
    float radiusSq = radius * radius;
    int z = (int)m->z;
    int mcx = (int)(m->x / CELL_SIZE), mcy = (int)(m->y / CELL_SIZE);

    int radCells = (int)ceilf(radius * moverGrid.invCellSize);
    int gx0 = (int)(m->x * moverGrid.invCellSize), gy0 = (int)(m->y * moverGrid.invCellSize);
    int minGx = gx0 - radCells < 0 ? 0 : gx0 - radCells;
    int minGy = gy0 - radCells < 0 ? 0 : gy0 - radCells;
    int maxGx = gx0 + radCells > moverGrid.gridW - 1 ? moverGrid.gridW - 1 : gx0 + radCells;
    int maxGy = gy0 + radCells > moverGrid.gridH - 1 ? moverGrid.gridH - 1 : gy0 + radCells;

    for (int gy = minGy; gy <= maxGy; gy++) {
        for (int gx = minGx; gx <= maxGx; gx++) {
            int cell = gy * moverGrid.gridW + gx;
            for (int t = moverGrid.cellStarts[cell]; t < moverGrid.cellStarts[cell + 1]; t++) {
                int j = moverGrid.moverIndices[t];
                if (j == i || !movers[j].active || (int)movers[j].z != z) continue;
                float dx = movers[j].x - m->x, dy = movers[j].y - m->y;
                float dSq = dx * dx + dy * dy;
                if (dSq > radiusSq) continue;
                int ocx = (int)(movers[j].x / CELL_SIZE), ocy = (int)(movers[j].y / CELL_SIZE);
                if (!HasLineOfSight(mcx, mcy, ocx, ocy, z)) continue;

                const NoticeEntry* old = FindEntry(n, j);
                float s = 1.0f - sqrtf(dSq) / radius;
                if (old) s += NOTICE_STICKY_BONUS;
                if (HeadingToward(j, m->x, m->y)) s += NOTICE_APPROACH_BONUS;

                // Insert into the top-N, most salient first
                int pos = fresh.count;
                while (pos > 0 && fresh.e[pos - 1].salience < s) pos--;
                if (pos >= NOTICE_MAX) continue;
                int last = fresh.count < NOTICE_MAX ? fresh.count : NOTICE_MAX - 1;
                for (int k = last; k > pos; k--) fresh.e[k] = fresh.e[k - 1];
                fresh.e[pos] = (NoticeEntry){ j, old ? old->since : gameTime, s };
                if (fresh.count < NOTICE_MAX) fresh.count++;
            }
        }
    }
    *n = fresh;
}

void NoticeTick(void) {
    if (!noticeEnabled) return;
    int every = noticeEveryTicks < 1 ? 1 : noticeEveryTicks;
    unsigned int phase = noticeTickCounter++ % (unsigned int)every;
    for (int i = (int)phase; i < moverCount; i += every) NoticeMover(i);
}
