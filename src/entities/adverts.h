// adverts.h - free time at home: things advertise what they offer (a spike of the advertisement
// system, docs/todo/dollhouse-domestic-life.md "Advertisement System")
//
// A resident at home with nothing to do (no need, no job, no talk, no visit) looks round its
// home: a sofa or a chair offers "sit and relax", a window "look out", a dirty floor "tidy me",
// any spot "potter about". Each offer (an advert) scores rate / (1 + distance / 10); the rate
// depends on the mover and the moment (tired: the sofa; Tidy: the dirt; after dark: not the
// window). It picks one of the best three at random (weighted), so two residents don't do the
// same thing, and does it as a short action queue (walk, sit, wait; tidy). Between activities it
// stays free a while (talks and visits start then); a need ends an activity at once, and a
// resident relaxing still answers the door (visit.c).
//
// Game rule homeLife (galerij). Only residents inside their own home (places.h) take part.
// Transient: a load ends the label (a running queue finishes as a plain queue).
#ifndef ADVERTS_H
#define ADVERTS_H

#include <stdbool.h>

typedef enum {
    ADV_RELAX,        // sit on a sofa / chair a while
    ADV_LOOK_OUT,     // stand at a window
    ADV_TIDY,         // clean a dirty bit of floor
    ADV_POTTER,       // walk to another spot and stand there
    ADV_KIND_COUNT
} AdvertKind;

extern bool homeLife;                     // game rule (galerij), off elsewhere
extern long advertStarted[ADV_KIND_COUNT]; // since ClearAdverts (stats)

void ClearAdverts(void);
void AdvertTick(float dt);                // sim_step.c, after the visits
bool AdvertBusy(int moverIdx);            // doing a free-time activity now
void AdvertStop(int moverIdx);            // stop it (the door: a visitor)
bool AdvertDescribe(int moverIdx, char* buf, int len);   // "Relaxing on the Sofa"
const char* AdvertKindName(int kind);

#endif // ADVERTS_H
