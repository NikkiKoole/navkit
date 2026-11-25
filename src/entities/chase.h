// chase.h - chasing and fleeing, and a game of tag on top (docs/todo/social/noticing.md)
//
// The building blocks are meant for animals later (a wolf chasing, a deer fleeing):
// - chase: run at where the target is going (its position + heading, leading more the further
//   away it is), path re-planned every CHASE_REPLAN s;
// - flee: when the threat is close, score candidate spots 5-9 cells away (far from the threat,
//   in a direction away from it, reachable) and run there along a real path; pick again when the
//   threat closes in or on arrival. fleeSmart off: straight away from the threat (how animals
//   flee today), to compare.
// Tag: players (ChaseSetPlayer) - "it" chases the nearest player it noticed; within catchDist
// the roles swap and the new "it" counts to three (stands still) before it chases.
#ifndef CHASE_H
#define CHASE_H

#include <stdbool.h>

extern bool fleeSmart;           // flee along paths to a good spot (false: straight away)
extern float chaseSpeed, fleeSpeed;   // running pace, x walking speed
extern float fleeRadius;         // cells: a threat this close makes a player run
extern long tagCount;            // tags since ClearChase
extern double tagChaseSecondsSum;   // sum of chase durations (for the average)

void ClearChase(void);
void ChaseSetPlayer(int moverIdx, bool playing);
bool ChaseIsPlaying(int moverIdx);
int ChaseIt(void);                       // who is "it", -1 if no game
void ChaseSetIt(int moverIdx);
void ChaseTick(float dt);                // after NoticeTick: decide who runs where
bool ChaseMoverTick(int moverIdx, float dt);   // movement loop: true = handled (counting to three)
float ChaseSpeedScale(int moverIdx);     // running pace for a chasing/fleeing player (1 otherwise)
bool ChaseIsFleeing(int moverIdx);
float ChaseStamina(int moverIdx);        // 0..1: a runner gets tired (worn out below 0.3: slow)

#endif
