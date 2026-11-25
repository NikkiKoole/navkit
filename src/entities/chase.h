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

// --- Hide and seek ---
// HideSeekStart(seeker, rounds): the seeker counts (eyes shut) at home; the others each pick a
// hiding spot 4-14 cells away (hideSmart: out of sight from home, tucked in by walls, away from
// the other hiders; off: any spot in range) and run there, then stand still. Then the seeker
// searches, remembering where it has looked (goes where it hasn't); a hider is found when the
// seeker notices it, and walks home. All found or the round's time up: the first one found seeks.
extern bool hideSmart;
extern long hideFoundCount, hideRounds;
extern double hideFoundSecondsSum;   // time from the end of counting to each find
void HideSeekStart(int seeker);
int HideSeeker(void);                // -1 if no game
bool HideIsCounting(int moverIdx);   // the seeker, eyes shut
bool HideIsHidden(int moverIdx);     // in its spot, not found yet
bool HideIsFound(int moverIdx);
bool HideControls(int moverIdx);     // a player in a game of hide and seek: the game gives its goals

// --- Follow the leader ---
// FollowLeaderStart(players, n): the first leads a winding route (legs 5-10 cells) at 0.8x; the
// others walk in a line, each following the trail of the one ahead (followTrail: the route it
// actually took, about a cell back; off: straight at the one ahead, cutting corners). Every
// 60 s the leader goes to the back and the next one leads.
#define FOLLOW_MAX 16
extern bool followTrail;
extern long followLeaderChanges;
void FollowLeaderStart(const int* players, int n);
int FollowLeader(void);                 // who leads now, -1 if no game
int FollowPlace(int moverIdx);          // its place in the line (0 = leader), -1 if not playing
bool FollowTrailDistance(int moverIdx, float* cells);   // how far a follower is from the leader's route

// --- Pack hunt ---
// PackHuntStart(hunters, n, target): n hunters (up to 8) on one runner. packTactics: roles
// re-assigned all the time: the nearest drives (straight at it), one cuts it off where it is
// going, the others flank (beside and ahead of it); off: all straight at it. The runner flees
// from all of them (away from the nearest, not past one), with stamina. Caught: the runner
// turns up again somewhere >= 10 cells from the hunters (a new round).
extern bool packTactics;
extern float packSpeed;          // hunters' running pace (x walking); the runner: fleeSpeed
extern long packCatches;
extern double packCatchSecondsSum;
void PackHuntStart(const int* hunters, int n, int target);
int PackTarget(void);            // -1 if no hunt
int PackRole(int moverIdx);      // 0 driver, 1 cutter, 2 flanker, -1 not hunting

#endif
