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
#include "activity.h"

// The games are joint activities (activity.h): each kind is a row there, its logic is here.
// Several games can run at once; the getters without a mover (ChaseIt, HideSeeker,
// FollowLeader, PackTarget, PackLost) answer for the first running game of that kind.
extern const ActivityDef tagActivityDef, hideSeekActivityDef, followLeaderActivityDef, packHuntActivityDef;
void ChaseMoverReset(int moverIdx);   // fresh for a game (stamina, not running): activity.c on joining
void ChaseMoverLeft(int moverIdx);    // out of its game (not running any more)

extern bool fleeSmart;           // flee along paths to a good spot (false: straight away)
extern float chaseSpeed, fleeSpeed;   // running pace, x walking speed
extern float fleeRadius;         // cells: a threat this close makes a player run
extern long tagCount;            // tags since ClearChase
extern double tagChaseSecondsSum;   // sum of chase durations (for the average)

void ClearChase(void);
// Playing is a free-time state (FREETIME_PLAY): a player gets no jobs and doesn't stop to talk.
// Only a free mover joins (no job, no need being seen to, not drafted). It leaves when something
// else takes it over (a need - hungry, thirsty, tired, bladder or anything critical, see
// PlayWantsToStop -, a job, drafting, a player command: its state is no longer FREETIME_PLAY),
// and the game carries on without it (a new "it", seeker or leader) or ends when too few are left.
// ChaseJoin = waits to play (activity.h ActivityWait): a game started next takes it
// (ChaseSetIt: tag, HideSeekStart: hide and seek); nothing started by the next tick: free again.
bool ChaseJoin(int moverIdx);            // false if it isn't free to play
void ChaseLeave(int moverIdx);
void ChaseSetPlayer(int moverIdx, bool playing);   // join / leave
bool PlayWantsToStop(int moverIdx);      // a need it should see to (needs.c ends its play)
// playLeftCount: activity.h
bool ChaseIsPlaying(int moverIdx);
int ChaseIt(void);                       // who is "it", -1 if no game
void ChaseSetIt(int moverIdx);            // "it" in its game of tag, or a new game with the ones waiting
void ChaseTick(float dt);                // after NoticeTick: decide who runs where
bool ChaseMoverTick(int moverIdx, float dt);   // movement loop: true = handled (counting to three)
float ChaseSpeedScale(int moverIdx);     // running pace for a chasing/fleeing player (1 otherwise)
bool ChaseIsFleeing(int moverIdx);
float ChaseStamina(int moverIdx);        // 0..1: a runner gets tired (worn out below 0.3: slow)

// --- Hide and seek ---
// HideSeekStart(seeker, rounds): the seeker counts (eyes shut) at home; the others each pick a
// hiding spot 4-14 cells away (hideSmart: out of sight from home - crouching behind low cover
// counts, see coverEnabled -, tucked in by walls and cover, away from the other hiders; off: any
// spot in range) and run there, then stand still. Then the seeker
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
bool HideControls(int moverIdx);     // a player in any game: the game gives its goals (the mover loop doesn't wander it)
bool MoverCrouching(int moverIdx);   // ducked down (a hider in its spot): low cover hides it (notice.h)

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
// packStealth: the pack only knows where one of them last saw the runner (12 cells, line of
// sight, not crouched behind cover). (Fleeing toward spots they can't see made it easier to catch:
// it ran into corners; dropped.) Out of their sight for 0.4 s, it lies low: a spot within 4 cells next to cover (or
// in a bush), hidden from every hunter and from where they'll come looking (where it was last seen
// and where it was heading); there it crouches and gets its breath back. Seen for 0.4 s (a glimpse
// isn't enough), or a hunter within 2.5 cells, and it runs again. The hunters, lost: first to
// where it was heading, then each searches spots round where it was last seen (next to cover, not
// in view of the pack, spread out). Lost 30 s: it got away (a new round). Off: they always know.
extern bool packStealth;
extern long packEscapes, packDucks, packFlushed;
extern double packDuckSecondsSum;   // time crouched in its spots
bool PackIsDucking(int moverIdx);       // the runner crouched in its spot
bool PackLost(float* seconds);          // the pack has lost sight of the runner (for how long)
extern bool packTactics;
extern float packSpeed;          // hunters' running pace (x walking)
extern float packRunnerSpeed;    // the runner's, fresh (worn out: slower)
extern long packCatches;
extern double packCatchSecondsSum;
void PackHuntStart(const int* hunters, int n, int target);
int PackTarget(void);            // -1 if no hunt
int PackRole(int moverIdx);      // 0 driver, 1 cutter, 2 flanker, 3 searching (lost it), -1 not hunting

#endif
