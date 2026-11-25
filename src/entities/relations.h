// relations.h - who knows whom, and how they feel about each other
// (docs/todo/social/noticing.md, step 2: relations)
//
// Each mover keeps up to MAX_RELATIONS others it knows: like (-100..100) and familiarity
// (0..100). One-way: what A feels about B, B keeps its own. An entry also stores the other's
// appearanceSeed, so a mover index reused after a death doesn't inherit the old feelings.
// When the list is full the least important entry (least familiar, weakest feeling) goes.
//
// Grows from: seeing each other (familiarity, slowly), talking (familiarity fast; like up in a
// good mood, down in a bad one), an "after you" at a doorway (like, both).
// Used by: who starts/joins a talk, who is noticed first, who goes first at a doorway.
// Saved (v111).
#ifndef RELATIONS_H
#define RELATIONS_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_RELATIONS 16

typedef struct {
    int other;            // mover index
    uint32_t otherSeed;   // movers[other].appearanceSeed when it was met (stale if different)
    float like;           // -100 .. 100
    float familiarity;    // 0 .. 100
} Relation;

typedef struct {
    Relation r[MAX_RELATIONS];
    int count;
} MoverRelations;

extern MoverRelations moverRelations[];   // MAX_MOVERS
extern float relSeenFamiliarityPerSec;    // familiarity gained per second of seeing someone
extern float relTalkFamiliarityPerSec;    // ... per second of talking with them
extern float relTalkLikePerSec;           // like per second of talking, at mood +0.7 (scaled by mood)

void ClearRelations(void);
void ClearMoverRelations(int moverIdx);           // a new mover in this slot knows nobody
const Relation* RelationGet(int a, int b);        // a's view of b; NULL if a doesn't know b
float RelationLike(int a, int b);                 // 0 if unknown
float RelationFamiliarity(int a, int b);          // 0 if unknown
void RelationAdd(int a, int b, float like, float familiarity);   // a's view of b changes by this
void RelationSetPair(int a, int b, float like, float familiarity);   // both ways, absolute
const char* RelationLabel(float like, float familiarity);        // "friend", "familiar face", ...

// Personal space: how close two stand or pass, as a scale on the normal distance (both views
// averaged): friends 0.6-0.7, someone you know 1, strangers 1.25, disliked up to 1.5. Used by
// talk circles (members' spacing), walking side by side, and polite passing (friends pass
// closer; never wider than normal: a stronger sideways drift pushed walkers through one-way doors).
// Setting personalSpace (off: 1 for everyone).
extern bool personalSpace;
float PersonalSpaceScale(int a, int b);

#endif
