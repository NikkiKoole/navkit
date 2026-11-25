// relations.c - who knows whom (see relations.h)
#include "relations.h"
#include "mover.h"
#include <string.h>
#include <math.h>

MoverRelations moverRelations[MAX_MOVERS];
float relSeenFamiliarityPerSec = 0.25f;   // ~7 minutes in sight from stranger to well known
float relTalkFamiliarityPerSec = 1.0f;
float relTalkLikePerSec = 0.3f;

void ClearRelations(void) {
    memset(moverRelations, 0, sizeof(moverRelations));
}

void ClearMoverRelations(int i) {
    if (i >= 0 && i < MAX_MOVERS) moverRelations[i].count = 0;
}

static bool Valid(const Relation* r) {
    return r->other >= 0 && r->other < moverCount && movers[r->other].active &&
           movers[r->other].appearanceSeed == r->otherSeed;
}

static Relation* Find(int a, int b) {
    if (a < 0 || a >= moverCount || b < 0 || b >= moverCount || a == b) return NULL;
    MoverRelations* mr = &moverRelations[a];
    for (int k = 0; k < mr->count; k++) {
        Relation* r = &mr->r[k];
        if (r->other == b) {
            if (Valid(r)) return r;
            *r = mr->r[--mr->count];   // stale (that slot is someone new now): forget it
            return NULL;
        }
    }
    return NULL;
}

// Least important entry: least familiar and weakest feeling; stale ones first
static int WeakestSlot(MoverRelations* mr) {
    int worst = 0;
    float worstScore = 1e9f;
    for (int k = 0; k < mr->count; k++) {
        const Relation* r = &mr->r[k];
        float s = Valid(r) ? r->familiarity + fabsf(r->like) * 2.0f : -1.0f;
        if (s < worstScore) { worstScore = s; worst = k; }
    }
    return worst;
}

static Relation* FindOrAdd(int a, int b) {
    Relation* r = Find(a, b);
    if (r || a < 0 || a >= moverCount || b < 0 || b >= moverCount || a == b) return r;
    MoverRelations* mr = &moverRelations[a];
    int k = mr->count < MAX_RELATIONS ? mr->count++ : WeakestSlot(mr);
    mr->r[k] = (Relation){ b, movers[b].appearanceSeed, 0.0f, 0.0f };
    return &mr->r[k];
}

const Relation* RelationGet(int a, int b) { return Find(a, b); }

float RelationLike(int a, int b) {
    const Relation* r = Find(a, b);
    return r ? r->like : 0.0f;
}

float RelationFamiliarity(int a, int b) {
    const Relation* r = Find(a, b);
    return r ? r->familiarity : 0.0f;
}

static float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool personalSpace = true;

float PersonalSpaceScale(int a, int b) {
    if (!personalSpace || a < 0 || b < 0) return 1.0f;
    float like = 0.5f * (RelationLike(a, b) + RelationLike(b, a));
    float fam = 0.5f * (RelationFamiliarity(a, b) + RelationFamiliarity(b, a));
    float s = 1.0f - like * 0.012f;                       // like +25: 0.7, -25: 1.3
    if (fam < 20.0f) s += 0.25f * (1.0f - fam / 20.0f);   // strangers keep a little more
    return Clampf(s, 0.6f, 1.5f);
}

void RelationAdd(int a, int b, float like, float familiarity) {
    Relation* r = FindOrAdd(a, b);
    if (!r) return;
    r->like = Clampf(r->like + like, -100.0f, 100.0f);
    r->familiarity = Clampf(r->familiarity + familiarity, 0.0f, 100.0f);
}

void RelationSetPair(int a, int b, float like, float familiarity) {
    Relation* r = FindOrAdd(a, b);
    if (r) { r->like = like; r->familiarity = familiarity; }
    r = FindOrAdd(b, a);
    if (r) { r->like = like; r->familiarity = familiarity; }
}

const char* RelationLabel(float like, float familiarity) {
    if (like >= 50.0f) return "friend";
    if (like <= -50.0f) return "enemy";
    if (like <= -20.0f) return "dislikes";
    if (like >= 20.0f) return "likes";
    if (familiarity >= 60.0f) return "knows well";
    if (familiarity >= 15.0f) return "familiar face";
    return "stranger";
}
