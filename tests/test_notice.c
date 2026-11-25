// test_notice.c - movers notice each other (src/entities/notice.c)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
static bool test_verbose = false;
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/notice.h"
#include "../src/entities/talk.h"
#include "../src/entities/relations.h"
#include "../src/entities/chase.h"
#include "../src/entities/action_queue.h"
#include "../src/entities/furniture.h"
#include "../src/simulation/fire.h"
#include "../src/entities/workshops.h"
#include "../src/core/time.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>
#include <math.h>

#define W 24
#define H 12

static void SetupWorld(void) {
    InitTestGrid(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
            GRID(x, y, 2) = CELL_AIR;
            SET_FLOOR(x, y, 1);
            SET_FLOOR(x, y, 2);
        }
    ClearMovers();
    InitMoverSpatialGrid(W * CELL_SIZE, H * CELL_SIZE);
    noticeEnabled = true;
    noticeRadius = 6.0f;
    noticeEveryTicks = 1;
    gameTime = 0.0;
}

static int AddMover(int cx, int cy, int cz) {
    int idx = moverCount++;
    Point goal = {cx, cy, cz};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f,
              (float)cz, goal, 100.0f);
    return idx;
}

static bool Notices(int i, int j) {
    for (int k = 0; k < moverNotice[i].count; k++) if (moverNotice[i].e[k].mover == j) return true;
    return false;
}

static void Look(void) {
    BuildMoverSpatialGrid();
    NoticeTick();
}

// Give mover i a one-waypoint path toward cell (gx, gy)
static void WalkToward(int i, int gx, int gy) {
    moverPaths[i][0] = (Point){gx, gy, (int)movers[i].z};
    movers[i].pathLength = 1;
    movers[i].pathIndex = 0;
}

static void Pass(int i, float hx, float hy, float* sx, float* sy, bool* any) {
    float scale;
    *any = NoticePassSteer(i, hx, hy, sx, sy, &scale);
}

describe(notice_basics) {
    it("two movers in view notice each other") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(8, 5, 1);
        Look();
        expect(Notices(a, b));
        expect(Notices(b, a));
        expect(NoticeTop(a) == b);
    }

    it("a wall between them hides them") {
        SetupWorld();
        for (int y = 0; y < H; y++) GRID(7, y, 1) = CELL_WALL;
        int a = AddMover(5, 5, 1), b = AddMover(9, 5, 1);
        Look();
        expect(!Notices(a, b));
        expect(NoticeTop(a) == -1);
    }

    it("another level is not noticed (v1)") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(6, 5, 2);
        Look();
        expect(!Notices(a, b));
    }

    it("beyond the radius is not noticed") {
        SetupWorld();
        int a = AddMover(2, 5, 1), b = AddMover(12, 5, 1);
        Look();
        expect(!Notices(a, b));
    }

    it("keeps at most NOTICE_MAX, the closest first") {
        SetupWorld();
        int a = AddMover(10, 5, 1);
        int near = AddMover(11, 5, 1);
        for (int k = 0; k < 6; k++) AddMover(13 + (k % 3), 4 + (k / 3) * 2, 1);
        Look();
        expect(moverNotice[a].count == NOTICE_MAX);
        expect(NoticeTop(a) == near);
    }

    it("attention sticks: an equal newcomer does not take the top spot") {
        SetupWorld();
        int a = AddMover(10, 5, 1), b = AddMover(13, 5, 1);
        Look();
        expect(NoticeTop(a) == b);
        int c = AddMover(7, 5, 1);   // same distance on the other side
        Look();
        expect(NoticeTop(a) == b);
        expect(Notices(a, c));
    }

    it("keeps the time it first noticed someone") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        gameTime = 10.0;
        Look();
        expect(moverNotice[a].e[0].mover == b);
        expect(moverNotice[a].e[0].since == 0.0);
    }

    it("noticing off: nobody notices") {
        SetupWorld();
        noticeEnabled = false;
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        (void)b;
        Look();
        expect(moverNotice[a].count == 0);
        expect(NoticeTop(a) == -1);
        noticeEnabled = true;
    }

    it("staggered: each mover looks once per noticeEveryTicks") {
        SetupWorld();
        noticeEveryTicks = 4;
        for (int k = 0; k < 8; k++) AddMover(4 + k, 5, 1);
        BuildMoverSpatialGrid();
        NoticeTick();
        int looked = 0;
        for (int i = 0; i < moverCount; i++) if (moverNotice[i].count > 0) looked++;
        expect(looked == 2);
        for (int t = 0; t < 3; t++) NoticeTick();
        looked = 0;
        for (int i = 0; i < moverCount; i++) if (moverNotice[i].count > 0) looked++;
        expect(looked == 8);
    }
}

describe(polite_passing) {
    it("head-on and dead ahead: both drift to their own right, opposite sides") {
        SetupWorld();
        politePassing = true;
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);   // 3 cells apart, inside passRadius
        WalkToward(a, 20, 5);   // east
        WalkToward(b, 0, 5);    // west
        Look();
        float ax, ay, bx, by; bool ga, gb;
        Pass(a, 1, 0, &ax, &ay, &ga);
        Pass(b, -1, 0, &bx, &by, &gb);
        expect(ga && gb);
        expect(ay > 0.0f);      // east-walker's right is south (y down)
        expect(by < 0.0f);      // west-walker's right is north
    }

    it("already offset: each moves away from the other's side") {
        SetupWorld();
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);
        movers[b].y -= CELL_SIZE * 0.5f;   // b a bit north = on a's left
        WalkToward(a, 20, 5);
        WalkToward(b, 0, 4);
        Look();
        float ax, ay, bx, by; bool ga, gb;
        Pass(a, 1, 0, &ax, &ay, &ga);
        Pass(b, -1, 0, &bx, &by, &gb);
        expect(ga && gb);
        expect(ay > 0.0f);      // a goes south, away from b
        expect(by < 0.0f);      // b goes north, away from a
    }

    it("same direction, behind, standing or far to the side: no giving way") {
        SetupWorld();
        int a = AddMover(4, 5, 1), same = AddMover(7, 5, 1), stand = AddMover(6, 6, 1),
            behind = AddMover(2, 5, 1), wide = AddMover(7, 8, 1);
        WalkToward(a, 20, 5);
        WalkToward(same, 20, 5);
        WalkToward(behind, 0, 5);
        WalkToward(wide, 0, 8);
        (void)stand;
        Look();
        float ax, ay; bool g;
        Pass(a, 1, 0, &ax, &ay, &g);
        expect(!g);
    }

    it("off: nothing") {
        SetupWorld();
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);
        WalkToward(a, 20, 5);
        WalkToward(b, 0, 5);
        Look();
        politePassing = false;
        float ax, ay; bool g;
        Pass(a, 1, 0, &ax, &ay, &g);
        expect(!g);
        politePassing = true;
    }
}

static void TalkSetup(void) {
    SetupWorld();
    ClearTalks();
    dayLength = 60.0f;
    talkEnabled = true;
    talkChance = 1.0f;
    talkStartRadius = 3.0f;
    talkMinSeconds = 5.0f;
    talkMaxSeconds = 5.0f;
    talkCooldown = 30.0f;
}

static void RunTalk(float seconds) {
    int ticks = (int)(seconds * TICK_RATE);
    for (int t = 0; t < ticks; t++) {
        TalkTick(TICK_DT);
        for (int i = 0; i < moverCount; i++) TalkMoverTick(i, TICK_DT);
    }
}

static float Dist(int a, int b) {
    float dx = movers[a].x - movers[b].x, dy = movers[a].y - movers[b].y;
    return sqrtf(dx * dx + dy * dy);
}

describe(stop_to_talk) {
    it("two idle movers close by start talking, stop walking and take spots 1.5 cells apart") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        WalkToward(a, 20, 5);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0 && MoverTalk(a) == MoverTalk(b));
        expect(movers[a].pathIndex < 0 && movers[a].pathLength == 0);
        expect(TalkPartner(a) == b && TalkPartner(b) == a);
        RunTalk(2.0f);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 2.0f);
        expect(MoverIsSpeaking(a) != MoverIsSpeaking(b));   // one speaks, one listens
    }

    it("ends after its time, then a cooldown keeps them from starting again at once") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0);
        RunTalk(5.0f);
        expect(MoverTalk(a) < 0 && MoverTalk(b) < 0);
        Look();
        RunTalk(2.0f);
        expect(MoverTalk(a) < 0);
        expect(talkStartedCount == 1);
    }

    it("a job ends it for that mover, and a talk of one ends") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0);
        movers[b].currentJobId = 0;
        RunTalk(0.1f);
        expect(MoverTalk(b) < 0);
        expect(MoverTalk(a) < 0);
        movers[b].currentJobId = -1;
    }

    it("a third joins: the three spread out round the middle, ~1.5 cells apart") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        TalkStart(a, b);
        int c = AddMover(6, 7, 1);
        Look();
        RunTalk(0.5f);
        int t = MoverTalk(a);
        expect(t >= 0 && MoverTalk(c) == t);
        expect(talks[t].count == 3);
        RunTalk(3.0f);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 3.0f);
        expect(fabsf(Dist(b, c) - 1.5f * CELL_SIZE) < 3.0f);
        expect(fabsf(Dist(a, c) - 1.5f * CELL_SIZE) < 3.0f);
    }

    it("one leaves a group of three: the other two close up to a pair") {
        TalkSetup();
        talkMinSeconds = talkMaxSeconds = 60.0f;   // long enough to outlast the test
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1), c = AddMover(6, 7, 1);
        TalkStart(a, b);
        TalkJoin(MoverTalk(a), c);
        RunTalk(3.0f);
        movers[c].currentJobId = 0;
        RunTalk(3.0f);
        expect(MoverTalk(c) < 0);
        expect(MoverTalk(a) >= 0 && talks[MoverTalk(a)].count == 2);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 3.0f);
        movers[c].currentJobId = -1;
    }

    it("a walker heading at a conversation steers round it; walking away or off: nothing") {
        TalkSetup();
        talkGiveRoom = true;
        int a = AddMover(10, 4, 1), b = AddMover(10, 6, 1), w = AddMover(7, 5, 1);   // 3 cells off
        TalkStart(a, b);
        float sx, sy;
        expect(TalkGiveRoomSteer(w, 1, 0, &sx, &sy));
        expect(sy > 0.0f);                              // dead ahead: goes round on its right (south)
        expect(!TalkGiveRoomSteer(w, -1, 0, &sx, &sy)); // walking away
        expect(!TalkGiveRoomSteer(a, 0, 1, &sx, &sy));  // a member doesn't steer round its own talk
        talkGiveRoom = false;
        expect(!TalkGiveRoomSteer(w, 1, 0, &sx, &sy));
        talkGiveRoom = true;
    }

    it("busy, too far or switched off: no talk") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1), far = AddMover(5, 10, 1);
        movers[b].currentJobId = 0;
        (void)far;
        Look();
        RunTalk(1.0f);
        expect(MoverTalk(a) < 0);
        movers[b].currentJobId = -1;
        talkEnabled = false;
        Look();
        RunTalk(1.0f);
        expect(MoverTalk(a) < 0);
        talkEnabled = true;
    }
}

describe(gatherings) {
    it("an idle mover in range comes to watch: a spot on the first ring, on its own side, facing in") {
        TalkSetup();
        watchChance = 1.0f;
        watchRadius = 6.0f;
        watchMinSeconds = watchMaxSeconds = 20.0f;
        int p = AddMover(12, 6, 1), w = AddMover(7, 6, 1);   // watcher 5 cells west
        expect(GatheringStart(p, 100.0f) >= 0);
        expect(MoverIsPerforming(p) && MoverIsSpeaking(p));
        RunTalk(0.5f);
        expect(MoverIsWatching(w) && TalkPartner(w) == p);
        RunTalk(4.0f);
        expect(fabsf(Dist(w, p) - 1.5f * CELL_SIZE) < 3.0f);
        expect(movers[w].x < movers[p].x);                    // stayed on the west side
        float lx, ly;
        expect(TalkLookAt(w, &lx, &ly) && fabsf(lx - movers[p].x) < 0.5f);
    }

    it("a wall between: does not come; out of range: does not come") {
        TalkSetup();
        watchChance = 1.0f;
        watchRadius = 4.0f;
        int p = AddMover(12, 6, 1), hidden = AddMover(9, 6, 1), far = AddMover(12, 1, 1);   // 3 and 5 cells
        for (int y = 0; y < H; y++) GRID(10, y, 1) = CELL_WALL;
        GatheringStart(p, 100.0f);
        RunTalk(1.0f);
        expect(!MoverIsWatching(hidden));
        expect(!MoverIsWatching(far));
        watchRadius = 6.0f;
    }

    it("onlookers leave after their time; the performer keeps performing") {
        TalkSetup();
        watchChance = 1.0f;
        watchMinSeconds = watchMaxSeconds = 2.0f;
        talkCooldown = 60.0f;
        int p = AddMover(12, 6, 1), w = AddMover(9, 6, 1);
        GatheringStart(p, 100.0f);
        RunTalk(0.5f);
        expect(MoverIsWatching(w));
        RunTalk(2.5f);
        expect(!MoverIsWatching(w));
        expect(MoverIsPerforming(p));
    }

    it("round a place (a campfire): they gather, take turns talking, and get to know each other") {
        TalkSetup();
        watchChance = 1.0f;
        int a = AddMover(9, 6, 1), b = AddMover(15, 6, 1);
        int t = GatheringStartAt(12.5f * CELL_SIZE, 6.5f * CELL_SIZE, 1, 100.0f, "the fire", 60.0f, 60.0f);
        expect(t >= 0 && talks[t].focus == -1);
        RunTalk(4.0f);
        expect(MoverIsWatching(a) && MoverIsWatching(b));
        expect(MoverIsSpeaking(a) != MoverIsSpeaking(b));   // one of them talks
        expect(RelationFamiliarity(a, b) > 1.0f);
        expect(strcmp(talks[t].what, "the fire") == 0);
    }

    it("doesn't join a gathering while someone it can't stand is there") {
        TalkSetup();
        watchChance = 1.0f;
        int a = AddMover(9, 6, 1), foe = AddMover(15, 6, 1);
        RelationAdd(a, foe, -40.0f, 50.0f);
        int t = GatheringStartAt(12.5f * CELL_SIZE, 6.5f * CELL_SIZE, 1, 100.0f, "the fire", 60.0f, 60.0f);
        (void)t;
        movers[a].currentJobId = 0;       // a busy for now: the foe gets there first
        RunTalk(1.0f);
        expect(MoverIsWatching(foe));
        movers[a].currentJobId = -1;      // now a is free, but the foe is at the fire
        RunTalk(2.0f);
        expect(!MoverIsWatching(a));
    }

    it("a full first ring: the next one goes to the second ring") {
        TalkSetup();
        watchChance = 1.0f;
        watchMinSeconds = watchMaxSeconds = 60.0f;
        int p = AddMover(12, 6, 1);
        int t = GatheringStart(p, 100.0f);
        for (int k = 0; k < 10; k++) AddMover(10 + (k % 5), 3 + (k / 5) * 6, 1);   // all within 4 cells
        Look();
        RunTalk(2.0f);
        int ring0 = 0, ring1 = 0;
        for (int k = 0; k < talks[t].count; k++) {
            if (talks[t].spot[k] / 64 == 0) ring0++;
            else if (talks[t].spot[k] / 64 == 1) ring1++;
        }
        expect(talks[t].count == 10);
        expect(ring0 == 9 && ring1 == 1);   // 2*pi*1.5 / 1.0 = 9 spots on the first ring
    }
}

describe(relations) {
    it("seeing each other makes them familiar, slowly") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(8, 5, 1);
        expect(RelationGet(a, b) == NULL);
        for (int t = 0; t < 60 * TICK_RATE; t++) Look();   // a minute in sight
        float f = RelationFamiliarity(a, b);
        expect(f > 14.0f && f < 16.0f);                    // 0.25 per second
        expect(RelationLike(a, b) == 0.0f);
        expect(strcmp(RelationLabel(RelationLike(a, b), f), "familiar face") == 0 ||
               strcmp(RelationLabel(RelationLike(a, b), f), "stranger") == 0);
    }

    it("talking: familiar fast; like goes up in a good mood, down in a bad one") {
        TalkSetup();
        talkMinSeconds = talkMaxSeconds = 60.0f;
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        movers[a].mood = 0.8f;
        movers[b].mood = -0.8f;
        TalkStart(a, b);
        RunTalk(10.0f);
        expect(RelationFamiliarity(a, b) > 9.0f);
        expect(RelationLike(a, b) > 2.0f);    // happy a likes b more
        expect(RelationLike(b, a) < -1.0f);   // grumpy b likes a less
    }

    it("never starts a talk with someone it dislikes, and won't join their talk") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        RelationAdd(a, b, -40.0f, 50.0f);
        Look();
        RunTalk(2.0f);
        expect(MoverTalk(a) < 0);
        int c = AddMover(6, 7, 1);
        RelationSetPair(b, c, 50.0f, 50.0f);
        TalkStart(b, c);
        Look();
        RunTalk(2.0f);
        expect(MoverTalk(a) < 0);             // a dislikes b: stays out of b and c's talk
    }

    it("a mover slot reused by someone new: the old feelings don't carry over") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        movers[b].appearanceSeed = 111;
        RelationSetPair(a, b, 70.0f, 80.0f);
        expect(RelationLike(a, b) == 70.0f);
        movers[b].appearanceSeed = 222;       // someone else in that slot now
        expect(RelationGet(a, b) == NULL);
        expect(moverRelations[a].count == 0);
    }

    it("a full list: the least important one makes room") {
        TalkSetup();
        int a = AddMover(1, 1, 1);
        int others[MAX_RELATIONS + 1];
        for (int k = 0; k <= MAX_RELATIONS; k++) {
            others[k] = AddMover(2 + k % 10, 2 + k / 10, 1);
            movers[others[k]].appearanceSeed = 1000 + k;
        }
        for (int k = 0; k < MAX_RELATIONS; k++) RelationAdd(a, others[k], 0.0f, 10.0f + k);
        RelationAdd(a, others[MAX_RELATIONS], 30.0f, 5.0f);   // new, strongly liked
        expect(moverRelations[a].count == MAX_RELATIONS);
        expect(RelationGet(a, others[0]) == NULL);            // least familiar, no feeling: gone
        expect(RelationLike(a, others[MAX_RELATIONS]) == 30.0f);
    }

    it("survives save and load") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        movers[a].appearanceSeed = 5; movers[b].appearanceSeed = 6;
        RelationSetPair(a, b, 42.0f, 77.0f);
        RelationAdd(b, a, -10.0f, 0.0f);
        expect(SaveWorld("/tmp/test_relations_save.bin"));
        ClearMovers();
        expect(RelationGet(a, b) == NULL);
        expect(LoadWorld("/tmp/test_relations_save.bin"));
        expect(RelationLike(a, b) == 42.0f && RelationFamiliarity(a, b) == 77.0f);
        expect(RelationLike(b, a) == 32.0f);
    }
}

describe(friends_and_foes) {
    it("a wanderer picks a goal next to a friend it knows (within 12 cells, even out of view)") {
        TalkSetup();
        socialSeek = true;
        int a = AddMover(2, 5, 1), f = AddMover(12, 5, 1);
        for (int y = 0; y < H; y++) if (y != 1) GRID(7, y, 1) = CELL_WALL;   // out of sight
        RelationAdd(a, f, 60.0f, 50.0f);
        bool got = false;
        int gx = 0, gy = 0, gz = 0;
        for (int t = 0; t < 50 && !got; t++) got = NoticeSocialWanderGoal(a, &gx, &gy, &gz);
        expect(got);
        expect(abs(gx - 12) <= 2 && abs(gy - 5) <= 2 && !(gx == 12 && gy == 5));
        RelationAdd(a, f, -50.0f, 0.0f);   // like 10: not enough
        got = false;
        for (int t = 0; t < 50 && !got; t++) got = NoticeSocialWanderGoal(a, &gx, &gy, &gz);
        expect(!got);
    }

    it("no wander goal near someone it dislikes; leaves when one comes close") {
        TalkSetup();
        socialAvoid = true;
        int a = AddMover(5, 5, 1), e = AddMover(7, 5, 1);
        RelationAdd(a, e, -40.0f, 50.0f);
        Look();
        expect(NoticeGoalNearDisliked(a, 9, 5, 1));    // 2 cells from them
        expect(!NoticeGoalNearDisliked(a, 20, 5, 1));  // far enough
        expect(NoticeDislikedClose(a));                // 2 cells: time to go
        expect(!NoticeDislikedClose(e));               // e has nothing against a
    }

    it("steers round someone disliked ahead, standing or walking; not round a friend") {
        TalkSetup();
        socialAvoid = true;
        int a = AddMover(5, 5, 1), e = AddMover(7, 5, 1), f = AddMover(5, 8, 1);
        (void)f;
        RelationAdd(a, e, -60.0f, 50.0f);
        Look();
        float sx, sy;
        expect(NoticeAvoidSteer(a, 1, 0, &sx, &sy));   // e dead ahead, standing
        expect(sy > 0.0f);                             // goes round on its right
        RelationAdd(a, e, 120.0f, 0.0f);               // now a friend (like 60)
        expect(!NoticeAvoidSteer(a, 1, 0, &sx, &sy));
        socialAvoid = false;
        RelationAdd(a, e, -120.0f, 0.0f);
        expect(!NoticeAvoidSteer(a, 1, 0, &sx, &sy));
        socialAvoid = true;
    }
}

// Walks go outdoors: open the sky over the test world (no floor on the level above)
static void WalkSetup(void) {
    TalkSetup();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) CLEAR_FLOOR(x, y, 2);
}

describe(walking_together) {
    it("a walk: the leader gets somewhere to go, the other stops its own walking") {
        WalkSetup();
        int a = AddMover(10, 6, 1), b = AddMover(11, 6, 1);
        WalkToward(b, 20, 6);
        expect(TalkWalkStart(a, b));
        expect(MoverIsWalkingWith(a) && MoverIsWalkingWith(b));
        expect(movers[a].pathLength > 0 && movers[a].pathIndex >= 0);
        expect(movers[b].pathIndex < 0);
        expect(TalkPartner(a) == b && TalkPartner(b) == a);
    }

    it("the other keeps a spot beside the leader; the leader slows when it falls behind") {
        WalkSetup();
        int a = AddMover(10, 6, 1), b = AddMover(13, 6, 1);   // b starts 3 cells off
        expect(TalkWalkStart(a, b));
        expect(TalkWalkSpeedScale(a) < walkPace);             // waits for b
        expect(TalkWalkSpeedScale(b) == 1.0f);                // (only the leader has a pace)
        for (int t = 0; t < 3 * TICK_RATE; t++) TalkMoverTick(b, TICK_DT);   // leader stands still here
        float dx = movers[b].x - movers[a].x, dy = movers[b].y - movers[a].y;
        expect(fabsf(sqrtf(dx * dx + dy * dy) - 0.8f * CELL_SIZE) < 3.0f);   // at its spot
        expect(TalkWalkSpeedScale(a) == walkPace);
    }

    it("indoors (a roof over them): no walk") {
        TalkSetup();   // floors on the level above: a roof
        int a = AddMover(10, 6, 1), b = AddMover(11, 6, 1);
        expect(!TalkWalkStart(a, b));
    }

    it("a group of four: everyone keeps its own spot round the leader") {
        WalkSetup();
        int a = AddMover(10, 6, 1), b = AddMover(11, 6, 1), c = AddMover(9, 7, 1), d = AddMover(10, 8, 1);
        expect(TalkWalkStart(a, b));
        int t = MoverTalk(a);
        expect(TalkWalkJoin(t, c) && TalkWalkJoin(t, d));
        expect(talks[t].count == 4);
        for (int s = 0; s < 4 * TICK_RATE; s++)
            for (int k = 1; k < 4; k++) TalkMoverTick(talks[t].members[k], TICK_DT);
        for (int k = 1; k < 4; k++) {
            float sx, sy;
            expect(TalkWalkSlot(t, k, &sx, &sy));
            const Mover* f = &movers[talks[t].members[k]];
            expect(sqrtf((f->x - sx) * (f->x - sx) + (f->y - sy) * (f->y - sy)) < 3.0f);
        }
        float s1x, s1y, s2x, s2y;
        TalkWalkSlot(t, 1, &s1x, &s1y); TalkWalkSlot(t, 2, &s2x, &s2y);
        expect(fabsf(s1x - s2x) + fabsf(s1y - s2y) > CELL_SIZE * 0.5f);   // different spots
    }

    it("one drops out of a group, the rest walk on") {
        WalkSetup();
        int a = AddMover(10, 6, 1), b = AddMover(11, 6, 1), c = AddMover(9, 7, 1);
        expect(TalkWalkStart(a, b));
        int t = MoverTalk(a);
        expect(TalkWalkJoin(t, c));
        movers[c].currentJobId = 0;
        TalkTick(TICK_DT);
        expect(!MoverIsWalkingWith(c));
        expect(MoverIsWalkingWith(a) && MoverIsWalkingWith(b) && talks[t].count == 2);
        movers[c].currentJobId = -1;
    }

    it("ends when they get separated or one gets a job") {
        WalkSetup();
        int a = AddMover(10, 6, 1), b = AddMover(11, 6, 1);
        expect(TalkWalkStart(a, b));
        movers[b].x += 6 * CELL_SIZE;                         // pulled apart
        TalkTick(TICK_DT);
        expect(!MoverIsWalkingWith(a) && !MoverIsWalkingWith(b));
        int c = AddMover(10, 8, 1), d = AddMover(11, 8, 1);
        expect(TalkWalkStart(c, d));
        movers[d].currentJobId = 0;
        TalkTick(TICK_DT);
        expect(!MoverIsWalkingWith(c));
        movers[d].currentJobId = -1;
    }
}

static void TagSetup(void) {
    TalkSetup();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) CLEAR_FLOOR(x, y, 2);
    fleeSmart = true;
    fleeRadius = 5.0f;
}

static void RunChase(float seconds) {
    int ticks = (int)(seconds * TICK_RATE);
    for (int t = 0; t < ticks; t++) {
        BuildMoverSpatialGrid();
        NoticeTick();
        ChaseTick(TICK_DT);
    }
}

describe(tag) {
    it("the new it counts to three before it chases") {
        TagSetup();
        int it = AddMover(5, 5, 1), p = AddMover(8, 5, 1);
        ChaseSetPlayer(it, true); ChaseSetPlayer(p, true);
        ChaseSetIt(it);
        expect(ChaseIt() == it);
        expect(ChaseMoverTick(it, TICK_DT));            // standing
        RunChase(1.6f);
        expect(!ChaseMoverTick(it, TICK_DT));           // off it goes
        expect(movers[it].pathIndex >= 0);              // a path at the player
    }

    it("a player runs when it comes close: to a spot further from it, away from it") {
        TagSetup();
        int it = AddMover(8, 6, 1), p = AddMover(11, 6, 1);
        ChaseSetPlayer(it, true); ChaseSetPlayer(p, true);
        ChaseSetIt(it);
        RunChase(0.5f);
        expect(ChaseIsFleeing(p));
        expect(movers[p].goal.x > 11);                  // it is to the west: runs east-ish
        float gx = (movers[p].goal.x + 0.5f) * CELL_SIZE - movers[it].x, gy = (movers[p].goal.y + 0.5f) * CELL_SIZE - movers[it].y;
        expect(sqrtf(gx * gx + gy * gy) > 5.0f * CELL_SIZE);
        expect(ChaseSpeedScale(p) == fleeSpeed && ChaseSpeedScale(it) == chaseSpeed);
    }

    it("running wears a player out; worn out it is slow") {
        TagSetup();
        int it = AddMover(8, 6, 1), p = AddMover(11, 6, 1);
        ChaseSetPlayer(it, true); ChaseSetPlayer(p, true);
        ChaseSetIt(it);
        movers[it].x = movers[p].x - 2.0f * CELL_SIZE;
        RunChase(7.0f);                                 // (nobody moves in this test: still close)
        expect(ChaseStamina(p) < 0.3f);
        expect(ChaseSpeedScale(p) < 1.0f);
    }

    it("caught: the roles swap") {
        TagSetup();
        int it = AddMover(8, 6, 1), p = AddMover(11, 6, 1);
        ChaseSetPlayer(it, true); ChaseSetPlayer(p, true);
        ChaseSetIt(it);
        RunChase(1.6f);                                 // done counting
        movers[it].x = movers[p].x - 0.3f * CELL_SIZE;  // right on top of p
        RunChase(0.1f);
        expect(tagCount == 1);
        expect(ChaseIt() == p);
        expect(ChaseMoverTick(p, TICK_DT));             // p counts to three now
    }

    it("not playing: never chased, never runs") {
        TagSetup();
        int it = AddMover(8, 6, 1), p = AddMover(11, 6, 1);
        ChaseSetPlayer(it, true);
        ChaseSetIt(it);
        RunChase(2.0f);
        expect(!ChaseIsFleeing(p));
        expect(ChaseSpeedScale(p) == 1.0f);
    }
}

describe(hide_and_seek) {
    it("smart hiding: the spot is out of sight of the seeker's home") {
        TagSetup();
        hideSmart = true;
        for (int y = 2; y <= 9; y++) GRID(14, y, 1) = CELL_WALL;   // a wall to hide behind
        int s = AddMover(8, 6, 1), h = AddMover(10, 6, 1);
        ChaseSetPlayer(s, true); ChaseSetPlayer(h, true);
        HideSeekStart(s);
        expect(HideSeeker() == s && HideIsCounting(s));
        expect(movers[h].pathIndex >= 0);                        // off to its spot
        Point g = movers[h].goal;
        expect(!HasLineOfSight(8, 6, g.x, g.y, 1));              // out of sight of home
    }

    it("counting: the seeker stands with its eyes shut; then it searches") {
        TagSetup();
        int s = AddMover(8, 6, 1), h = AddMover(10, 6, 1);
        ChaseSetPlayer(s, true); ChaseSetPlayer(h, true);
        HideSeekStart(s);
        expect(ChaseMoverTick(s, TICK_DT));
        RunChase(10.5f);
        expect(!HideIsCounting(s));
        expect(!ChaseMoverTick(s, TICK_DT) || movers[s].pathIndex >= 0);
    }

    it("a hider in plain sight gets found and walks home") {
        TagSetup();
        for (int y = 0; y < H; y++) GRID(16, y, 1) = CELL_WALL;  // someone else far behind a wall
        int s = AddMover(8, 6, 1), h = AddMover(10, 6, 1), other = AddMover(20, 6, 1);
        ChaseSetPlayer(s, true); ChaseSetPlayer(h, true); ChaseSetPlayer(other, true);
        HideSeekStart(s);
        RunChase(10.5f);                                         // counted; nobody moved (no movement here)
        expect(!HideIsFound(other));
        expect(HideIsFound(h));                                  // h never left: right there
        expect(hideFoundCount >= 1);
        expect(movers[h].goal.x == 8 && movers[h].goal.y == 6); // home
    }
}

describe(glances_and_greetings) {
    it("two who know each other meet: both greet (friends say hi), and it warms them a little") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        RelationSetPair(a, b, 60.0f, 50.0f);
        Look();
        float p;
        expect(NoticeGreeting(a, &p) == GREET_HI);
        expect(NoticeGreeting(b, &p) == GREET_HI);
        expect(NoticeGlanceAt(a, NULL) == b);
        expect(RelationLike(a, b) > 60.0f);
        int c = AddMover(5, 9, 1), d = AddMover(7, 9, 1);
        RelationSetPair(c, d, 10.0f, 30.0f);   // acquaintances: a nod
        Look();
        expect(NoticeGreeting(c, &p) == GREET_NOD);
    }

    it("strangers don't greet; someone disliked: it looks away") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        float p;
        Look();
        expect(NoticeGreeting(a, &p) == GREET_NONE);
        int c = AddMover(5, 9, 1), e = AddMover(7, 9, 1);
        RelationAdd(c, e, -40.0f, 50.0f);
        Look();
        bool away = false;
        expect(NoticeGlanceAt(c, &away) == e && away);
        expect(NoticeGreeting(c, &p) == GREET_AWAY);
        (void)b;
    }

    it("no second greeting while they stay in sight of each other") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        RelationSetPair(a, b, 20.0f, 50.0f);
        Look();
        long first = greetCount;
        expect(first == 1);
        movers[b].x += 20 * CELL_SIZE;          // out of notice for a moment ...
        for (int t = 0; t < 10; t++) Look();
        movers[b].x -= 20 * CELL_SIZE;          // ... and back within a minute
        for (int t = 0; t < 10; t++) Look();
        expect(greetCount == first);
    }

    it("switched off: no glances, no greetings") {
        TalkSetup();
        glancesEnabled = greetingsEnabled = false;
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        RelationSetPair(a, b, 60.0f, 50.0f);
        Look();
        float p;
        expect(NoticeGreeting(a, &p) == GREET_NONE && NoticeGlanceAt(a, NULL) < 0);
        glancesEnabled = greetingsEnabled = true;
    }
}

// A mover sitting on chair f, in the middle of eating (as sit-then-eat leaves it)
static int DinerAt(int f) {
    int i = AddMover(furniture[f].x, furniture[f].y, 1);
    QueueReset(i);
    QueuePush(i, ActEatHeld());
    moverQueues[i].seat = f;
    QueueStart(i, QUEUE_FROM_FREETIME);
    return i;
}

describe(table_talk) {
    it("two eating at the same table talk, look at the table and get to know each other") {
        TalkSetup();
        ClearFurniture();
        int table = SpawnFurniture(10, 6, 1, FURNITURE_TABLE, MAT_OAK);
        int c1 = SpawnFurniture(9, 6, 1, FURNITURE_CHAIR, MAT_OAK), c2 = SpawnFurniture(11, 6, 1, FURNITURE_CHAIR, MAT_OAK);
        int a = DinerAt(c1), b = DinerAt(c2);
        TalkTick(TICK_DT);
        expect(MoverDiningAt(a) == table && MoverDiningAt(b) == table);
        expect(MoverIsSpeaking(a) != MoverIsSpeaking(b));
        float lx, ly;
        expect(TalkLookAt(a, &lx, &ly) && fabsf(lx - 10.5f * CELL_SIZE) < 0.5f);
        for (int t = 0; t < 5 * TICK_RATE; t++) TalkTick(TICK_DT);
        expect(RelationFamiliarity(a, b) > 1.5f);
    }

    it("alone at a table: nobody to talk to") {
        TalkSetup();
        ClearFurniture();
        SpawnFurniture(10, 6, 1, FURNITURE_TABLE, MAT_OAK);
        int c1 = SpawnFurniture(9, 6, 1, FURNITURE_CHAIR, MAT_OAK);
        int a = DinerAt(c1);
        TalkTick(TICK_DT);
        expect(!MoverIsSpeaking(a));
    }
}

describe(spot_choice) {
    it("a seat next to a friend scores higher, next to a stranger a little lower, next to a foe much lower") {
        TalkSetup();
        ClearFurniture();
        spotByFriends = true;
        int me = AddMover(2, 2, 1);
        int f1 = SpawnFurniture(5, 5, 1, FURNITURE_CHAIR, MAT_OAK), n1 = SpawnFurniture(6, 5, 1, FURNITURE_CHAIR, MAT_OAK);
        int f2 = SpawnFurniture(5, 9, 1, FURNITURE_CHAIR, MAT_OAK), n2 = SpawnFurniture(6, 9, 1, FURNITURE_CHAIR, MAT_OAK);
        int f3 = SpawnFurniture(15, 5, 1, FURNITURE_CHAIR, MAT_OAK), n3 = SpawnFurniture(16, 5, 1, FURNITURE_CHAIR, MAT_OAK);
        int empty = SpawnFurniture(15, 9, 1, FURNITURE_CHAIR, MAT_OAK);
        int friendM = AddMover(20, 1, 1), stranger = AddMover(21, 1, 1), foe = AddMover(22, 1, 1);
        RelationAdd(me, friendM, 60.0f, 80.0f);
        RelationAdd(me, foe, -50.0f, 60.0f);
        furniture[n1].occupant = friendM;   // reserved: on their way to it
        furniture[n2].occupant = stranger;
        furniture[n3].occupant = foe;
        float pf = SeatPreference(me, f1), ps = SeatPreference(me, f2), pe = SeatPreference(me, f3), p0 = SeatPreference(me, empty);
        expect(pf > 2.0f);
        expect(ps < 0.0f && ps > -1.0f);
        expect(pe < -4.0f);
        expect(p0 == 0.0f);
        spotByFriends = false;
        expect(SeatPreference(me, f1) == 0.0f);
        spotByFriends = true;
    }

    it("round the fire: a newcomer takes the spot next to its friend") {
        TalkSetup();
        watchChance = 1.0f;
        int t = GatheringStartAt(12.5f * CELL_SIZE, 6.5f * CELL_SIZE, 1, 100.0f, "the fire", 60.0f, 60.0f);
        int friendM = AddMover(12, 2, 1);          // comes from the north first
        RunTalk(4.0f);
        expect(MoverIsWatching(friendM));
        int me = AddMover(6, 6, 1);                // comes from the west
        RelationAdd(me, friendM, 70.0f, 80.0f);
        RunTalk(4.0f);
        expect(MoverIsWatching(me));
        float dx = movers[me].x - movers[friendM].x, dy = movers[me].y - movers[friendM].y;
        expect(sqrtf(dx * dx + dy * dy) < 1.6f * CELL_SIZE);   // right beside the friend, not on the west side
        (void)t;
    }
}

describe(mingling) {
    it("in a talk with strangers, a friend's group nearby: moves over and joins it") {
        TalkSetup();
        mingleEnabled = true; mingleChance = 1.0f;
        talkMinSeconds = talkMaxSeconds = 120.0f;
        int a = AddMover(4, 5, 1), s1 = AddMover(6, 5, 1);      // a talks with a stranger
        int f = AddMover(12, 5, 1), f2 = AddMover(14, 5, 1);    // its friend talks with someone else
        RelationAdd(a, f, 60.0f, 80.0f);
        TalkStart(a, s1);
        TalkStart(f, f2);
        int other = MoverTalk(f);
        RunTalk(7.0f);                                          // a has been there 6 s+: leaves
        expect(MoverIsMingling(a) || MoverTalk(a) == other);
        for (int t = 0; t < 8 * TICK_RATE && MoverTalk(a) != other; t++) {   // walk over (by hand here)
            float dx = talks[other].centerX - movers[a].x, dy = talks[other].centerY - movers[a].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 1.0f) { movers[a].x += dx / d * 2.0f; movers[a].y += dy / d * 2.0f; }
            TalkTick(TICK_DT);
        }
        expect(MoverTalk(a) == other);
        expect(talks[other].count == 3);
        expect(mingleCount == 1);
        mingleChance = 0.02f;
    }

    it("nothing better nearby (or off, or a foe there): stays") {
        TalkSetup();
        mingleChance = 1.0f;
        talkMinSeconds = talkMaxSeconds = 120.0f;
        int a = AddMover(4, 5, 1), b = AddMover(6, 5, 1), c = AddMover(12, 5, 1), d = AddMover(14, 5, 1);
        RelationSetPair(a, b, 40.0f, 60.0f);
        RelationAdd(a, c, 50.0f, 60.0f);                        // only a bit better than here: stays
        TalkStart(a, b); TalkStart(c, d);
        RunTalk(8.0f);
        expect(MoverTalk(a) == MoverTalk(b) && !MoverIsMingling(a));
        RelationAdd(a, c, 30.0f, 0.0f);                         // now much better, but a foe is there
        RelationAdd(a, d, -40.0f, 50.0f);
        RunTalk(4.0f);
        expect(MoverTalk(a) == MoverTalk(b));
        mingleEnabled = false;
        RelationAdd(a, d, 80.0f, 0.0f);
        RunTalk(4.0f);
        expect(MoverTalk(a) == MoverTalk(b));
        mingleEnabled = true; mingleChance = 0.02f;
    }
}

static int CountWatchGatherings(void) {
    int n = 0;
    for (int t = 0; t < MAX_TALKS; t++) if (talks[t].active && talks[t].kind == TALK_WATCH) n++;
    return n;
}

describe(auto_gatherings) {
    it("a burning barrel: a gathering starts round it; the barrel gone: it ends; off: none") {
        TalkSetup();
        ClearFurniture();
        autoGatherings = true;
        int f = SpawnFurniture(10, 6, 1, FURNITURE_FIRE_BARREL, MAT_IRON);
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 1);
        expect(autoGatheringsStarted == 1);
        furniture[f].active = false;
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 0);
        autoGatherings = false;
        furniture[f].active = true;
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 0);
    }

    it("a fire workshop only while it burns") {
        TalkSetup();
        ClearFurniture();
        ClearWorkshops();
        autoGatherings = true;
        int ws = CreateWorkshop(10, 6, 1, WORKSHOP_GROUND_FIRE);
        expect(ws >= 0);
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 0);              // not lit
        SetFireSource(workshops[ws].fuelTileX, workshops[ws].fuelTileY, 1, true);
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 1);
        SetFireSource(workshops[ws].fuelTileX, workshops[ws].fuelTileY, 1, false);
        ExtinguishCell(workshops[ws].fuelTileX, workshops[ws].fuelTileY, 1);   // (no fire sim here to burn it down)
        TalkTick(1.1f);
        expect(CountWatchGatherings() == 0);              // gone out
        autoGatherings = false;
    }

    it("people come to it by themselves (the barrel blocks its own cell)") {
        TalkSetup();
        ClearFurniture();
        autoGatherings = true;
        watchChance = 1.0f;
        SpawnFurniture(10, 6, 1, FURNITURE_FIRE_BARREL, MAT_IRON);
        int a = AddMover(6, 6, 1);
        Look();
        RunTalk(2.0f);
        expect(MoverIsWatching(a));
        autoGatherings = false;
    }
}

// Distance (pixels) from (x, y) to the L route: (x0,y0) east to (x1,y0), then south to (x1,y1)
static float DistToL(float x, float y, float x0, float x1, float y0, float y1) {
    float dA = (x < x0 ? x0 - x : (x > x1 ? x - x1 : 0.0f)) + fabsf(y - y0);   // the east leg (city-block, fine here)
    float dB = fabsf(x - x1) + (y < y0 ? y0 - y : (y > y1 ? y - y1 : 0.0f));  // the south leg
    return dA < dB ? dA : dB;
}

// The leader walks an L by hand (2 px a tick), the follower follows; worst distance from the L
static float RunFollowL(bool trail) {
    TagSetup();
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) CLEAR_FLOOR(x, y, 2);
    followTrail = trail;
    int lead = AddMover(4, 3, 1), fol = AddMover(3, 3, 1);
    int players[2] = { lead, fol };
    FollowLeaderStart(players, 2);
    float x0 = movers[lead].x, y0 = movers[lead].y, x1 = x0 + 4 * CELL_SIZE, y1 = y0 + 4 * CELL_SIZE;
    float worst = 0.0f;
    for (int t = 0; t < 600; t++) {
        if (movers[lead].x < x1) movers[lead].x += 2.0f; else if (movers[lead].y < y1) movers[lead].y += 2.0f;
        movers[lead].pathLength = 1; movers[lead].pathIndex = 0;   // (keep it "walking": no new legs)
        moverPaths[lead][0] = (Point){(int)(x1 / CELL_SIZE), (int)(y1 / CELL_SIZE), 1};
        ChaseTick(TICK_DT);
        ChaseMoverTick(fol, TICK_DT);
        if (movers[fol].x >= x0) {
            float d = DistToL(movers[fol].x, movers[fol].y, x0, x1, y0, y1);
            if (d > worst) worst = d;
        }
    }
    followTrail = true;
    return worst / CELL_SIZE;
}

describe(follow_the_leader) {
    it("following the trail: the follower walks the leader's L, round the corner") {
        expect(RunFollowL(true) < 0.2f);
    }

    it("straight at the one ahead instead: it cuts the corner") {
        expect(RunFollowL(false) > 0.3f);
    }

    it("the leader leads, the others are in line behind it") {
        TagSetup();
        int a = AddMover(4, 3, 1), b = AddMover(3, 3, 1), c = AddMover(2, 3, 1);
        int players[3] = { a, b, c };
        FollowLeaderStart(players, 3);
        expect(FollowLeader() == a && FollowPlace(b) == 1 && FollowPlace(c) == 2);
        expect(HideControls(b) && ChaseIsPlaying(c));   // the game gives their goals
        expect(ChaseSpeedScale(a) < 1.0f);              // the leader goes a bit slower
    }
}

describe(pack_hunt) {
    it("sight: across a railing or a window yes, through a wall no") {
        TalkSetup();
        GRID(8, 5, 1) = CELL_RAILING;
        GRID(8, 7, 1) = CELL_WINDOW;
        GRID(8, 9, 1) = CELL_WALL;
        expect(CanSeeCell(5, 5, 11, 5, 1));
        expect(CanSeeCell(5, 7, 11, 7, 1));
        expect(!CanSeeCell(5, 9, 11, 9, 1));
        expect(!HasLineOfSight(5, 5, 11, 5, 1));   // (walking straight across it: no)
    }

    it("roles: the nearest drives, one cuts off, the rest flank") {
        TagSetup();
        packTactics = true;
        int h[4];
        h[0] = AddMover(10, 4, 1); h[1] = AddMover(4, 4, 1); h[2] = AddMover(4, 8, 1); h[3] = AddMover(16, 8, 1);
        int t = AddMover(11, 6, 1);
        for (int k = 0; k < 4; k++) ChaseSetPlayer(h[k], true);
        ChaseSetPlayer(t, true);
        PackHuntStart(h, 4, t);
        RunChase(0.2f);
        int drivers = 0, cutters = 0, flankers = 0;
        for (int k = 0; k < 4; k++) {
            int r = PackRole(h[k]);
            drivers += r == 0; cutters += r == 1; flankers += r == 2;
        }
        expect(drivers == 1 && cutters == 1 && flankers == 2);
        expect(PackRole(h[0]) == 0);                       // the nearest
        expect(ChaseSpeedScale(h[1]) == packSpeed);
    }

    it("the runner flees between two hunters: away from both, not past one") {
        TagSetup();
        int a = AddMover(4, 6, 1), b = AddMover(10, 2, 1), t = AddMover(8, 6, 1);
        int hs[2] = { a, b };
        ChaseSetPlayer(a, true); ChaseSetPlayer(b, true); ChaseSetPlayer(t, true);
        PackHuntStart(hs, 2, t);
        Look();
        RunChase(0.3f);
        expect(ChaseIsFleeing(t));
        expect(movers[t].goal.x > 8 && movers[t].goal.y > 4);   // east and south: away from a (west) and b (north)
    }

    it("caught: a new round, the runner turns up far from the pack") {
        TagSetup();
        int h[1] = { AddMover(8, 6, 1) };
        int t = AddMover(8, 6, 1);
        ChaseSetPlayer(h[0], true); ChaseSetPlayer(t, true);
        PackHuntStart(h, 1, t);
        RunChase(0.1f);
        expect(packCatches == 1);
        float dx = movers[t].x - movers[h[0]].x, dy = movers[t].y - movers[h[0]].y;
        expect(sqrtf(dx * dx + dy * dy) >= 10.0f * CELL_SIZE);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(notice_basics);
    test(polite_passing);
    test(stop_to_talk);
    test(gatherings);
    test(relations);
    test(friends_and_foes);
    test(walking_together);
    test(tag);
    test(hide_and_seek);
    test(glances_and_greetings);
    test(table_talk);
    test(spot_choice);
    test(mingling);
    test(auto_gatherings);
    test(follow_the_leader);
    test(pack_hunt);
    return summary();
}
