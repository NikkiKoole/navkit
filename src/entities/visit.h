// visit.h - visiting a friend at home (infrastructure step 2: the galerij showcase;
// docs/todo/world/infrastructure.md, docs/todo/architecture/ownership-and-access.md)
//
// A visit is a joint activity (activity.c, kind ACT_VISIT). The guest walks to the friend's
// doorstep (outside the front door of the friend's home place) and knocks. Someone of that
// household who is at home and free comes to the door, opens it and lets the guest in: the guest
// gets a grant for the home (places.h), so it may come in and use things there. They stay a while
// (company: social.c counts activity members), then the guest leaves; the host closes the door
// behind it or not (Tidy hosts always, others by visitCloseChance). Nobody home, asleep or busy:
// the guest knocks a while and gives up. A guest whose visit ends walks home; its grant is taken
// back once it is outside that home (never a mover stranded inside without one).
//
// Movers visit by themselves when autoVisits is on (a game rule; the galerij scenario): a free
// resident at home, now and then (visitsPerDay), visits the household member elsewhere it likes
// best (at least visitMinLike). Transient like every activity: a load ends visits.
#ifndef VISIT_H
#define VISIT_H

#include <stdbool.h>
#include "activity.h"

extern const ActivityDef visitActivityDef;

extern bool autoVisits;
extern float visitsPerDay;       // chance per free mover per day (spread over its checks)
extern float visitMinLike;       // how much it must like someone to visit
extern float visitWaitHours;     // game hours it knocks before giving up
extern float visitStayMinHours, visitStayMaxHours;
extern float visitCooldownHours; // game hours before it visits again
extern float visitCloseChance;   // a host who isn't Tidy closes the door behind the guest

extern long visitStarted, visitAnswered, visitGivenUp, visitDone;
extern long visitCutShort;          // a need (or a job, a command) took the guest away

void ClearVisits(void);
void VisitTick(float dt);              // after ChaseTick: propose visits, walk former guests home
int  VisitStart(int guest, int friendMover);   // activity index, -1 if it can't start
bool VisitDescribe(int i, char* buf, int len); // "Visiting Flat 3: knocking"
bool MoverIsVisiting(int i);           // a guest in a visit now
bool MoverIsHosting(int i);

#endif // VISIT_H
