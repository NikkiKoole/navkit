// sim_step.c - One game tick (see sim_step.h)
#include "sim_step.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/items.h"
#include "../entities/animals.h"
#include "../entities/notice.h"
#include "../entities/talk.h"
#include "../entities/chase.h"
#include "../entities/social.h"
#include "../entities/trains.h"
#include "../entities/belts.h"
#include "../entities/town.h"
#include "../entities/jobs.h"
#include "../entities/throwing.h"
#include "../world/designations.h"
#include "../simulation/needs.h"
#include "../simulation/mood.h"
#include "../simulation/explosion.h"
#include "../simulation/vapour.h"
#include "../world/support.h"
#include "effects.h"
#include "../entities/adverts.h"
#include "../entities/visit.h"
#include "../world/mechanisms.h"
#include "../world/pumps.h"

void SimStep(float dt) {
    // Time, cell simulations, lighting, rooms, plants, spatial grids, movement
    PROFILE_BEGIN(TickWithDt);
    TickWithDt(dt);
    PROFILE_END(TickWithDt);
    // dt is one real tick; game time (dt * gameSpeed, 0 when paused) is what the slow clocks need:
    // spoilage, the town's housekeeping and imports, moodlets. They ran at 1x at any game speed,
    // so at 10x a season went by while food spoiled, towns restocked and crops grew 10x too slow.
    float gameDt = gameDeltaTime;

    PROFILE_BEGIN(Items);
    ItemsTick(gameDt);
    ThrowsTick(dt);
    ExplosionsTick(dt);
    VapourTick(dt);
    PROFILE_BEGIN(Support);
    SupportTick(dt);
    PROFILE_END(Support);
    FxTick(dt);   // visual effects age on game time (frozen while paused)
    PROFILE_END(Items);

    PROFILE_BEGIN(Notice);
    NoticeTick();
    TalkTick(dt);
    ChaseTick(dt);
    VisitTick(dt);  // visits: setting off, former guests walking home (visit.h)
    AdvertTick(dt); // free time at home: relax, look out, tidy, potter (adverts.h)
    SocialTick();   // company: up in talks and games, down alone (social.h)
    PROFILE_END(Notice);

    PROFILE_BEGIN(Animals);
    AnimalsTick(dt);
    PROFILE_END(Animals);

    PROFILE_BEGIN(Trains);
    TrainsTick(dt);
    PROFILE_END(Trains);

    PROFILE_BEGIN(Belts);
    BeltsTick(dt);
    PROFILE_END(Belts);

    PROFILE_BEGIN(Town);
    TownTick(gameDt);
    PROFILE_END(Town);

    PROFILE_BEGIN(Designations);
    DesignationsTick(dt);
    PROFILE_END(Designations);

    MechanismsTick();   // pressure plates, doors waiting for their doorway to clear (mechanisms.h)
    PumpsTick(dt);      // pump drawing timers (pumps.h)

    PROFILE_BEGIN(Needs);
    PROFILE_BEGIN(NeedsTick); NeedsTick(); PROFILE_END(NeedsTick);
    PROFILE_BEGIN(Freetime); ProcessFreetimeNeeds(); PROFILE_END(Freetime);
    PROFILE_BEGIN(Mood); MoodTick(gameDt); PROFILE_END(Mood);
    PROFILE_END(Needs);

    PROFILE_BEGIN(Jobs);
    AssignJobs();
    JobsTick();
    PROFILE_END(Jobs);
}

// Not part of SimStep: ValidateSimActivityCounts (the windowed loop runs it every
// 60 s) is a debug safety net that reads every cell of every layer, too costly for
// big headless runs and benchmarks.
