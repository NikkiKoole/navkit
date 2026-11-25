// social.h - the social need: company (docs/todo/social/joint-activities.md "Starting")
//
// moverSocial[i]: 1 = had plenty of company, 0 = very lonely. Awake and with nobody, it goes
// down (socialDrainGH game hours from full to empty); in company it goes up (socialGainGH from
// empty to full), faster with friends, slower with strangers, not at all with someone it
// dislikes. Company = a talk, a gathering, a walk together, table talk, a game.
// Only where people have a social life: the game rule talkEnabled (and the setting socialNeed),
// and someone else in the world (a lone survivor isn't lonely for what it can't change).
// Low: the moodlets Lonely / Very lonely; after time together: Good company (mood.c).
// Saved (v120): one float per mover after the purses; older saves: everyone 1.
#ifndef SOCIAL_H
#define SOCIAL_H
#include <stdbool.h>
#include "mover.h"

extern float moverSocial[MAX_MOVERS];
extern bool socialNeedEnabled;   // setting socialNeed (also needs talkEnabled)
extern float socialDrainGH;      // game hours from full to empty, awake and alone
extern float socialGainGH;       // game hours from empty to full, in a friend's company

#define SOCIAL_LONELY        0.3f   // below: Lonely
#define SOCIAL_VERY_LONELY   0.1f   // below: Very lonely
#define SOCIAL_GOOD_COMPANY  0.8f   // above, in company: Good company

void SocialResetAll(void);       // everyone 1 (new world, older save)
void SocialResetMover(int moverIdx);
bool SocialActive(void);         // the need runs (socialNeed + talkEnabled + 2 or more movers)
void SocialTick(void);           // after the talks and games (uses gameDeltaTime)
// Who it is with right now (talk, gathering, walk, table, game): fills out[] (up to max),
// returns how many; 0 = nobody
int SocialCompany(int moverIdx, int* out, int max);
// Company gain multiplier for i with these others: from how much it likes them (0 = disliked)
float SocialCompanyScale(int moverIdx, const int* others, int n);

#endif // SOCIAL_H
