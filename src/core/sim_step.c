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
#include "../world/support.h"
#include "effects.h"

void SimStep(float dt) {
    // Time, cell simulations, lighting, rooms, plants, spatial grids, movement
    PROFILE_BEGIN(TickWithDt);
    TickWithDt(dt);
    PROFILE_END(TickWithDt);

    PROFILE_BEGIN(Items);
    ItemsTick(dt);
    ThrowsTick(dt);
    ExplosionsTick(dt);
    PROFILE_BEGIN(Support);
    SupportTick(dt);
    PROFILE_END(Support);
    FxTick(dt);   // visual effects age on game time (frozen while paused)
    PROFILE_END(Items);

    PROFILE_BEGIN(Notice);
    NoticeTick();
    TalkTick(dt);
    ChaseTick(dt);
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
    TownTick(dt);
    PROFILE_END(Town);

    PROFILE_BEGIN(Designations);
    DesignationsTick(dt);
    PROFILE_END(Designations);

    PROFILE_BEGIN(Needs);
    PROFILE_BEGIN(NeedsTick); NeedsTick(); PROFILE_END(NeedsTick);
    PROFILE_BEGIN(Freetime); ProcessFreetimeNeeds(); PROFILE_END(Freetime);
    PROFILE_BEGIN(Mood); MoodTick(dt); PROFILE_END(Mood);
    PROFILE_END(Needs);

    PROFILE_BEGIN(Jobs);
    AssignJobs();
    JobsTick();
    PROFILE_END(Jobs);
}

// Not part of SimStep: ValidateSimActivityCounts (the windowed loop runs it every
// 60 s) is a debug safety net that reads every cell of every layer, too costly for
// big headless runs and benchmarks.
