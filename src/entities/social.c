// social.c - the social need: company (see social.h)
#include "social.h"
#include "talk.h"
#include "activity.h"
#include "relations.h"
#include "../core/time.h"
#include "../simulation/balance.h"
#include <string.h>

float moverSocial[MAX_MOVERS];
bool socialNeedEnabled = false;   // off until lonely movers go out to find company (joint-activities step 2b): town citizens ended at 0% (all very lonely, mood 53% -> 45%)
float socialDrainGH = 24.0f;   // a day awake on its own: lonely (0.3) after ~17 hours
float socialGainGH = 1.0f;     // an hour with a friend: from very lonely to plenty

#define SOCIAL_EVERY 10        // ticks between updates per mover (staggered by index)

void SocialResetMover(int i) { if (i >= 0 && i < MAX_MOVERS) moverSocial[i] = 1.0f; }
void SocialResetAll(void) { for (int i = 0; i < MAX_MOVERS; i++) moverSocial[i] = 1.0f; }
// Someone to have company with: a lone survivor isn't punished for what it can't change
static bool socialOthers = false;   // two or more active movers (counted each SocialTick)
bool SocialActive(void) { return socialNeedEnabled && talkEnabled && socialOthers; }

static void SocialAdd(int* out, int* n, int max, int j) {
    for (int k = 0; k < *n; k++) if (out[k] == j) return;
    if (*n < max) out[(*n)++] = j;
}

int SocialCompany(int i, int* out, int max) {
    int n = 0;
    if (i < 0 || i >= moverCount || !movers[i].active) return 0;
    int t = MoverTalk(i);   // a talk, a gathering (onlookers, the one they watch), a walk
    if (t >= 0) {
        const Talk* tk = &talks[t];
        for (int k = 0; k < tk->count; k++) if (tk->members[k] != i) SocialAdd(out, &n, max, tk->members[k]);
        if (tk->focus >= 0 && tk->focus != i && tk->focus < moverCount && movers[tk->focus].active) SocialAdd(out, &n, max, tk->focus);
    }
    int table = MoverDiningAt(i);   // eating at a table with others
    if (table >= 0)
        for (int j = 0; j < moverCount; j++) if (j != i && MoverDiningAt(j) == table) SocialAdd(out, &n, max, j);
    int a = MoverActivity(i);   // a game
    if (a >= 0)
        for (int j = 0; j < moverCount; j++) if (j != i && ActivityHas(a, j)) SocialAdd(out, &n, max, j);
    return n;
}

// Stranger (like 0): x0.5, friend (like 50): x1, close (100): x1.5, disliked: nothing
float SocialCompanyScale(int i, const int* others, int n) {
    if (n <= 0) return 0.0f;
    float like = 0.0f;
    for (int k = 0; k < n; k++) like += RelationLike(i, others[k]);
    float s = 0.5f + (like / n) / 100.0f;
    return s < 0.0f ? 0.0f : (s > 1.5f ? 1.5f : s);
}

void SocialTick(void) {
    int alive = 0;
    for (int i = 0; i < moverCount && alive < 2; i++) if (movers[i].active) alive++;
    socialOthers = alive >= 2;
    if (!SocialActive()) return;
    float dt = gameDeltaTime * SOCIAL_EVERY;
    float drain = dt / GameHoursToGameSeconds(socialDrainGH);
    float gain = dt / GameHoursToGameSeconds(socialGainGH);
    for (int i = (int)(currentTick % SOCIAL_EVERY); i < moverCount; i += SOCIAL_EVERY) {
        const Mover* m = &movers[i];
        if (!m->active) continue;
        int others[16];
        int n = SocialCompany(i, others, 16);
        float v = moverSocial[i];
        if (n > 0) v += gain * SocialCompanyScale(i, others, n);
        else if (m->freetimeState != FREETIME_RESTING) v -= drain;   // (asleep: neither)
        moverSocial[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
}
